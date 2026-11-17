// Regression for a COM compatibility shim wrapping an already-installed hook.
#include <windows.h>
#include <d3d8.h>
#include <cstdio>
#include <cstdlib>
using Release=ULONG (WINAPI *)(IDirect3DDevice8*);
static Release next_release=nullptr;
static unsigned depth=0,calls=0;
static bool recursed=false;
static ULONG WINAPI layered_release(IDirect3DDevice8* device) {
    ++calls;if(++depth>4){recursed=true;--depth;return 1000;}
    const ULONG result=next_release(device);--depth;return result;
}
static void check(bool value,const char* text){if(!value){std::fprintf(stderr,"FAIL: %s\n",text);std::exit(1);}}
static LRESULT CALLBACK procedure(HWND w,UINT m,WPARAM a,LPARAM b){return DefWindowProcA(w,m,a,b);}
int main() {
    WNDCLASSA cls={};cls.hInstance=GetModuleHandleA(nullptr);cls.lpfnWndProc=procedure;cls.lpszClassName="OSMLayeredReleaseRegression";RegisterClassA(&cls);
    HWND window=CreateWindowA(cls.lpszClassName,"OpenSoccerMania COM regression",WS_OVERLAPPEDWINDOW,0,0,640,480,nullptr,nullptr,cls.hInstance,nullptr);
    HMODULE module=LoadLibraryA(".\\d3d8.dll");check(module!=nullptr,"Load bridge");
    auto factory=reinterpret_cast<IDirect3D8* (WINAPI *)(UINT)>(GetProcAddress(module,"Direct3DCreate8"));check(factory!=nullptr,"D3D8 factory");
    IDirect3D8* api=factory(D3D_SDK_VERSION);check(api!=nullptr,"Create API");
    D3DPRESENT_PARAMETERS params={};params.BackBufferWidth=640;params.BackBufferHeight=480;params.BackBufferFormat=D3DFMT_X8R8G8B8;params.BackBufferCount=1;params.SwapEffect=D3DSWAPEFFECT_DISCARD;params.hDeviceWindow=window;params.Windowed=TRUE;
    IDirect3DDevice8 *first=nullptr,*second=nullptr;
    check(SUCCEEDED(api->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,&params,&first)),"Create first device");
    auto** table=*reinterpret_cast<void***>(first);next_release=reinterpret_cast<Release>(table[2]);
    DWORD protection=0;check(VirtualProtect(table+2,sizeof(void*),PAGE_READWRITE,&protection)!=FALSE,"Layer Release");
    table[2]=reinterpret_cast<void*>(layered_release);VirtualProtect(table+2,sizeof(void*),protection,&protection);
    check(SUCCEEDED(api->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,&params,&second)),"Create second device with layered COM slot");
    check(!recursed,"Device creation must not replace its original with a forwarding shim");
    // dgVoodoo 2.87.6 rebuilds its shared table during CreateDevice; older
    // versions preserve the shim. In either case the bridge must still be the
    // downstream target. Restore only the exact known shim if it was reset.
    const auto layer=[&](IDirect3DDevice8* device) {
        auto** current=*reinterpret_cast<void***>(device);
        if(current[2]==reinterpret_cast<void*>(layered_release))return;
        check(current[2]==reinterpret_cast<void*>(next_release),"Backend table reset must retain bridge Release");
        DWORD previous=0;check(VirtualProtect(current+2,sizeof(void*),PAGE_READWRITE,&previous)!=FALSE,"Restore Release layer after backend table reset");
        current[2]=reinterpret_cast<void*>(layered_release);VirtualProtect(current+2,sizeof(void*),previous,&previous);
    };
    layer(first);layer(second);
    const ULONG second_refs=second->Release();check(!recursed,"Layered Release must not recurse");check(second_refs==0,"Second device releases completely");
    const ULONG first_refs=first->Release();check(!recursed&&first_refs==0,"First device releases completely through shim");
    check(calls>0,"Compatibility shim remains in call chain");api->Release();DestroyWindow(window);
    std::puts("PASS: layered COM Release chain, second device and complete cleanup");
}
