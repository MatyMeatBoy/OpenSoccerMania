// Keep legacy DirectInput acquisition local to a focused game window.
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <cstdio>
#include <cstring>

using Factory=HRESULT (WINAPI *)(HINSTANCE,DWORD,REFIID,void**,LPUNKNOWN);
using CreateDevice=HRESULT (WINAPI *)(void*,REFGUID,void**,LPUNKNOWN);
using Cooperative=HRESULT (WINAPI *)(void*,HWND,DWORD);
static HMODULE self_module;
static CreateDevice original_create=nullptr;
static Cooperative original_cooperative=nullptr;
static bool exclusive=false;
static char log_path[MAX_PATH];
using DeviceState=HRESULT (WINAPI *)(void*,DWORD,LPVOID);
static DeviceState original_state=nullptr;
static HWND game_window=nullptr;
static HANDLE input_mapping=nullptr;
static const unsigned char* input_memory=nullptr;
static bool keyboard_state_logged=false;
static HRESULT WINAPI device_state(void* device,DWORD size,LPVOID data) {
    HRESULT result=original_state(device,size,data);
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
    if(words[0]!=0x494d534f || (sequence&1) || !(flags&1))return result;
    if(!(flags&2) && GetForegroundWindow()!=game_window)return result;
    unsigned char keys[256];std::memcpy(keys,input_memory+16,256);MemoryBarrier();
    if(words[1]!=sequence)return result;
    if(FAILED(result))std::memset(data,0,256);
    auto* state=static_cast<unsigned char*>(data);
    for(unsigned i=0;i<256;++i)state[i]|=keys[i];
    return DI_OK;
}

static void replace(void** slot,void* replacement,void** original) {
    if(*slot==replacement)return;
    DWORD protection;
    if(VirtualProtect(slot,sizeof(void*),PAGE_READWRITE,&protection)) {
        *original=*slot;*slot=replacement;
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
    HRESULT result=original_cooperative(device,window,flags);
    FILE* file=std::fopen(log_path,"a");
    if(file){std::fprintf(file,"cooperative requested=0x%lx applied=0x%lx result=0x%lx\n",requested,flags,static_cast<unsigned long>(result));std::fclose(file);}
    return result;
}

static HRESULT WINAPI create_device(void* api,REFGUID guid,void** result,LPUNKNOWN outer) {
    HRESULT status=original_create(api,guid,result,outer);
    if(SUCCEEDED(status) && result && *result) {
        auto table=*reinterpret_cast<void***>(*result);
        replace(&table[13],reinterpret_cast<void*>(cooperative),reinterpret_cast<void**>(&original_cooperative));
        replace(&table[9],reinterpret_cast<void*>(device_state),reinterpret_cast<void**>(&original_state));
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
    }
    return status;
}

BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH){self_module=module;DisableThreadLibraryCalls(module);}
    return TRUE;
}
