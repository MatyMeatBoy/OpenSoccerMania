#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <cstdio>
#include <cstring>

int main(int argc,char** argv) {
    if(argc!=2)return 2;
    HMODULE module=LoadLibraryA(argv[1]);if(!module)return 3;
    using Factory=HRESULT (WINAPI *)(HINSTANCE,DWORD,REFIID,void**,LPUNKNOWN);
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
    words[1]=3;std::memset(bytes+16,0,256);words[1]=4;
    std::memset(state,0,256);keyboard->GetDeviceState(256,state);
    if(state[DIK_S] || state[DIK_UP])return 10;
    keyboard->Release();api->Release();DestroyWindow(window);UnmapViewOfFile(bytes);CloseHandle(mapping);
    std::puts("DirectInput private mailbox: press/release passed without desktop input.");return 0;
}
