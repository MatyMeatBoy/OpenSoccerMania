// Keep legacy DirectInput acquisition local to a focused game window.
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <cstdio>
#include <cstring>
#include "ui_state.h"
#include "hook_chain.h"
#include "virtual_pad.h"
#include <unordered_map>

using Factory=HRESULT (WINAPI *)(HINSTANCE,DWORD,REFIID,void**,LPUNKNOWN);
using CreateDevice=HRESULT (WINAPI *)(void*,REFGUID,void**,LPUNKNOWN);
using Cooperative=HRESULT (WINAPI *)(void*,HWND,DWORD);
static HMODULE self_module;
static CreateDevice original_create=nullptr;
static Cooperative original_cooperative=nullptr;
static bool exclusive=false;
static bool diagnostic_keyboard=false;
static char log_path[MAX_PATH];
using DeviceState=HRESULT (WINAPI *)(void*,DWORD,LPVOID);
static DeviceState original_state=nullptr;
static HWND game_window=nullptr;
static HANDLE input_mapping=nullptr;
static const unsigned char* input_memory=nullptr;
static bool keyboard_state_logged=false;
static osm::UiStateReader ui_state;
static osm::PadReader pad_reader;
using EnumDevices=HRESULT (WINAPI *)(void*,DWORD,LPDIENUMDEVICESCALLBACKA,LPVOID,DWORD);
static EnumDevices original_enum=nullptr;
static int pad_route=0; // decide once, avoiding duplicate routes if discovery is late
static bool virtual_pad_route() {
    if(pad_route)return pad_route>0;
    const auto deadline=GetTickCount64()+300;osm::PadPage page={};
    do {if(pad_reader.snapshot(page)){pad_route=1;FILE* file=std::fopen(log_path,"a");if(file){std::fprintf(file,"CONTROLLER_ROUTE SDL-four-device protocol=1 keyboard-emulation=0\n");std::fclose(file);}return true;}Sleep(5);}while(GetTickCount64()<deadline);
    pad_route=-1;FILE* file=std::fopen(log_path,"a");if(file){std::fprintf(file,"CONTROLLER_ROUTE native-fallback reason=SDL-page-unavailable\n");std::fclose(file);}return false; // SDL unavailable: preserve the original native devices
}
static std::unordered_map<void**,DeviceState> state_methods;
static std::unordered_map<void**,Cooperative> cooperative_methods;
struct EnumFilter {LPDIENUMDEVICESCALLBACKA callback;LPVOID context;bool stopped=false;};
static BOOL CALLBACK enum_nonpads(LPCDIDEVICEINSTANCEA info,LPVOID context) {
    auto& filter=*static_cast<EnumFilter*>(context);
    const DWORD kind=LOBYTE(info->dwDevType);
    if(kind!=DI8DEVTYPE_KEYBOARD&&kind!=DI8DEVTYPE_MOUSE)return DIENUM_CONTINUE;
    const BOOL result=filter.callback(info,filter.context);
    if(result==DIENUM_STOP)filter.stopped=true;
    return result;
}
static HRESULT WINAPI enum_devices(void* api,DWORD kind,LPDIENUMDEVICESCALLBACKA callback,LPVOID context,DWORD flags) {
    if(!callback)return DIERR_INVALIDPARAM;
    const bool wants_pads=kind==DI8DEVCLASS_ALL||kind==DI8DEVCLASS_GAMECTRL||kind==DI8DEVTYPE_GAMEPAD||kind==DI8DEVTYPE_JOYSTICK;
    if(wants_pads&&!virtual_pad_route())return original_enum(api,kind,callback,context,flags);
    EnumFilter filter{callback,context};
    const HRESULT result=original_enum(api,kind,enum_nonpads,&filter,flags);
    if(FAILED(result)||filter.stopped)return result;
    if(!wants_pads)return result;
    for(unsigned slot=0;slot<4;++slot) {
        osm::PadState state={};if(!pad_reader.read(slot,state))continue;
        DIDEVICEINSTANCEA info={};osm::pad_info(slot,info);
        if(callback(&info,context)==DIENUM_STOP)break;
    }
    return DI_OK;
}
static HRESULT WINAPI device_state(void* device,DWORD size,LPVOID data) {
    DeviceState poll=original_state;
    if(device){const auto method=state_methods.find(*reinterpret_cast<void***>(device));if(method!=state_methods.end())poll=method->second;}
    HRESULT result=poll(device,size,data);
    // Poll the real device first, preserving acquisition/focus side effects.
    // Only recognized keyboard/mouse packet shapes have an all-zero neutral state.
    if(data && (size==256 || size==sizeof(DIMOUSESTATE) || size==sizeof(DIMOUSESTATE2)) && ui_state.panel_open()) {
        std::memset(data,0,size);
        return DI_OK;
    }
    if(size!=256 || !data)return result;
    if(!keyboard_state_logged){FILE* file=std::fopen(log_path,"a");if(file){std::fprintf(file,"KEYBOARD_STATE bytes=256 result=0x%lx\n",static_cast<unsigned long>(result));std::fclose(file);}keyboard_state_logged=true;}
    if(!input_memory) {
        char name[96];std::snprintf(name,sizeof(name),"Local\\OpenSoccerMania-Input-%lu",GetCurrentProcessId());
        input_mapping=OpenFileMappingA(FILE_MAP_READ,FALSE,name);
        if(input_mapping)input_memory=static_cast<const unsigned char*>(MapViewOfFile(input_mapping,FILE_MAP_READ,0,0,272));
    }
    if(!input_memory)return result;
    const auto* words=reinterpret_cast<const volatile DWORD*>(input_memory);
    const DWORD sequence=words[1],flags=words[2];
    const auto unstable=[&](){if(diagnostic_keyboard){std::memset(data,0,256);return DI_OK;}return result;};
    if(words[0]!=0x494d534f || (sequence&1))return unstable();
    MemoryBarrier();if(words[1]!=sequence)return unstable();
    diagnostic_keyboard=(flags&3)==3;
    if(!(flags&1))return result;
    const bool diagnostic=(flags&2)!=0;
    const bool movie_skip=(flags&16)&&ui_state.movie_active();
    if(!diagnostic&&!movie_skip)return result;
    if(!diagnostic&&GetForegroundWindow()!=game_window)return result;
    unsigned char keys[256];std::memcpy(keys,input_memory+16,256);MemoryBarrier();
    if(words[1]!=sequence)return unstable();
    if(diagnostic||FAILED(result))std::memset(data,0,256);
    auto* state=static_cast<unsigned char*>(data);
    for(unsigned i=0;i<256;++i)if(diagnostic||i==DIK_ESCAPE)state[i]|=keys[i];
    return DI_OK;
}

static void replace(void** slot,void* replacement,void** original) {
    if(osm::hook_decision(*slot,replacement,*original)!=osm::HookDecision::Install)return;
    DWORD protection;
    if(VirtualProtect(slot,sizeof(void*),PAGE_READWRITE,&protection)) {
        if(!*original)*original=*slot;*slot=replacement;
        VirtualProtect(slot,sizeof(void*),protection,&protection);
    }
}

static HRESULT WINAPI cooperative(void* device,HWND window,DWORD flags) {
    game_window=window;
    const DWORD requested=flags;
    if(!exclusive) {
        flags &= ~(DISCL_EXCLUSIVE|DISCL_BACKGROUND|DISCL_NOWINKEY);
        flags |= DISCL_NONEXCLUSIVE|DISCL_FOREGROUND;
        ClipCursor(nullptr);ReleaseCapture();
    }
    Cooperative call=original_cooperative;
    const auto method=cooperative_methods.find(*reinterpret_cast<void***>(device));if(method!=cooperative_methods.end())call=method->second;
    HRESULT result=call(device,window,flags);
    FILE* file=std::fopen(log_path,"a");
    if(file){std::fprintf(file,"cooperative requested=0x%lx applied=0x%lx result=0x%lx\n",requested,flags,static_cast<unsigned long>(result));std::fclose(file);}
    return result;
}

static HRESULT WINAPI create_device(void* api,REFGUID guid,void** result,LPUNKNOWN outer) {
    const int slot=osm::pad_guid_slot(guid);
    if(slot>=0) {
        if(!result)return E_POINTER;*result=nullptr;
        if(outer)return CLASS_E_NOAGGREGATION;
        osm::PadState state={};if(!pad_reader.read(unsigned(slot),state))return DIERR_DEVICENOTREG;
        auto* pad=new(std::nothrow) osm::VirtualPad(unsigned(slot),pad_reader,&ui_state);
        if(!pad)return E_OUTOFMEMORY;*result=static_cast<IDirectInputDevice8A*>(pad);return DI_OK;
    }
    HRESULT status=original_create(api,guid,result,outer);
    if(SUCCEEDED(status) && result && *result) {
        auto table=*reinterpret_cast<void***>(*result);
        using Caps=HRESULT (WINAPI *)(void*,LPDIDEVCAPS);
        DIDEVCAPS caps={};caps.dwSize=sizeof(caps);
        if(SUCCEEDED(reinterpret_cast<Caps>(table[3])(*result,&caps))) {
            const DWORD kind=LOBYTE(caps.dwDevType);
            if(kind!=DI8DEVTYPE_KEYBOARD&&kind!=DI8DEVTYPE_MOUSE&&virtual_pad_route()) {
                reinterpret_cast<ULONG (WINAPI *)(void*)>(table[2])(*result);*result=nullptr;return DIERR_DEVICENOTREG;
            }
        }
        if(cooperative_methods.find(table)==cooperative_methods.end()) {
            Cooperative call=nullptr;DeviceState poll=nullptr;
            replace(&table[13],reinterpret_cast<void*>(cooperative),reinterpret_cast<void**>(&call));
            replace(&table[9],reinterpret_cast<void*>(device_state),reinterpret_cast<void**>(&poll));
            cooperative_methods[table]=call;state_methods[table]=poll;
            if(!original_cooperative)original_cooperative=call;if(!original_state)original_state=poll;
        }
    }
    return status;
}

extern "C" HRESULT WINAPI DirectInput8Create(HINSTANCE instance,DWORD version,REFIID iid,void** result,LPUNKNOWN outer) {
    char directory[MAX_PATH],path[MAX_PATH],mode[32];
    GetModuleFileNameA(self_module,directory,MAX_PATH);
    char* slash=std::strrchr(directory,'\\');if(!slash)return E_FAIL;slash[1]=0;
    std::snprintf(path,MAX_PATH,"%sopensoccermania.ini",directory);
    std::snprintf(log_path,MAX_PATH,"%sopensoccermania-input.log",directory);
    GetPrivateProfileStringA("Graphics","DisplayMode","Windowed",mode,sizeof(mode),path);
    exclusive=std::strcmp(mode,"Fullscreen")==0;
    GetSystemDirectoryA(path,MAX_PATH);std::strcat(path,"\\dinput8.dll");
    HMODULE backend=LoadLibraryExA(path,nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);
    auto factory=backend?reinterpret_cast<Factory>(GetProcAddress(backend,"DirectInput8Create")):nullptr;
    if(!factory)return E_FAIL;
    HRESULT status=factory(instance,version,iid,result,outer);
    if(SUCCEEDED(status)&&result&&*result) {
        auto table=*reinterpret_cast<void***>(*result);
        replace(&table[3],reinterpret_cast<void*>(create_device),reinterpret_cast<void**>(&original_create));
        replace(&table[4],reinterpret_cast<void*>(enum_devices),reinterpret_cast<void**>(&original_enum));
    }
    return status;
}

BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH){self_module=module;DisableThreadLibraryCalls(module);}
    return TRUE;
}
