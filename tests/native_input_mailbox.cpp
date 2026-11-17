#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <cstdio>
#include <cstring>
static unsigned physical_calls=0;
static HRESULT WINAPI physical_keyboard(void*,DWORD size,LPVOID data){
    if(size!=256||!data)return DIERR_INVALIDPARAM;
    ++physical_calls;std::memset(data,0,size);static_cast<BYTE*>(data)[DIK_SPACE]=0x80;return DI_OK;
}

int main(int argc,char** argv) {
    if(argc!=2)return 2;
    using Factory=HRESULT (WINAPI *)(HINSTANCE,DWORD,REFIID,void**,LPUNKNOWN);
    char system[MAX_PATH];GetSystemDirectoryA(system,MAX_PATH);std::strcat(system,"\\dinput8.dll");
    HMODULE system_module=LoadLibraryA(system);if(!system_module)return 20;
    auto system_factory=reinterpret_cast<Factory>(GetProcAddress(system_module,"DirectInput8Create"));
    IDirectInput8A* physical_api=nullptr;IDirectInputDevice8A* physical_device=nullptr;
    if(!system_factory||FAILED(system_factory(GetModuleHandleA(nullptr),DIRECTINPUT_VERSION,IID_IDirectInput8A,reinterpret_cast<void**>(&physical_api),nullptr)))return 21;
    if(FAILED(physical_api->CreateDevice(GUID_SysKeyboard,&physical_device,nullptr)))return 22;
    auto** table=*reinterpret_cast<void***>(physical_device);DWORD protection=0;
    if(!VirtualProtect(table+9,sizeof(void*),PAGE_READWRITE,&protection))return 23;
    table[9]=reinterpret_cast<void*>(physical_keyboard);VirtualProtect(table+9,sizeof(void*),protection,&protection);
    HMODULE module=LoadLibraryA(argv[1]);if(!module)return 3;
    auto factory=reinterpret_cast<Factory>(GetProcAddress(module,"DirectInput8Create"));if(!factory)return 4;
    IDirectInput8A* api=nullptr;IDirectInputDevice8A* keyboard=nullptr;
    if(FAILED(factory(GetModuleHandleA(nullptr),DIRECTINPUT_VERSION,IID_IDirectInput8A,reinterpret_cast<void**>(&api),nullptr)))return 5;
    if(FAILED(api->CreateDevice(GUID_SysKeyboard,&keyboard,nullptr)))return 6;
    HWND window=CreateWindowExA(0,"STATIC","Mailbox integration test",WS_OVERLAPPEDWINDOW,0,0,200,100,nullptr,nullptr,GetModuleHandleA(nullptr),nullptr);
    keyboard->SetDataFormat(&c_dfDIKeyboard);
    if(FAILED(keyboard->SetCooperativeLevel(window,DISCL_EXCLUSIVE|DISCL_FOREGROUND)))return 7;
    char name[96];std::snprintf(name,sizeof(name),"Local\\OpenSoccerMania-Input-%lu",GetCurrentProcessId());
    HANDLE mapping=CreateFileMappingA(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,272,name);
    auto* bytes=static_cast<unsigned char*>(MapViewOfFile(mapping,FILE_MAP_WRITE,0,0,272));
    auto* words=reinterpret_cast<DWORD*>(bytes);
    std::memset(bytes,0,272);words[0]=0x494d534f;words[1]=2;words[2]=3;
    bytes[16+DIK_S]=0x80;bytes[16+DIK_UP]=0x80;
    unsigned char state[256]={};
    if(FAILED(keyboard->GetDeviceState(256,state)))return 8;
    if(state[DIK_S]!=0x80 || state[DIK_UP]!=0x80 || state[DIK_D])return 9;
    if(state[DIK_SPACE]){std::fputs("FAIL: physical key leaked into private diagnostic state\n",stderr);return 24;}
    words[1]=3;keyboard->GetDeviceState(256,state);
    for(unsigned char key:state)if(key)return 27; // Unstable private writes remain neutral.
    words[1]=4;
    words[1]=3;std::memset(bytes+16,0,256);words[1]=4;
    std::memset(state,0,256);keyboard->GetDeviceState(256,state);
    if(state[DIK_S] || state[DIK_UP])return 10;
    if(state[DIK_SPACE]||!physical_calls)return 25;
    words[2]=1;keyboard->GetDeviceState(256,state);
    if(state[DIK_SPACE]!=0x80||state[DIK_S])return 26; // Production keeps physical input.
    keyboard->Release();api->Release();physical_device->Release();physical_api->Release();DestroyWindow(window);UnmapViewOfFile(bytes);CloseHandle(mapping);
    std::puts("DirectInput private mailbox: press/release passed without desktop input.");return 0;
}
