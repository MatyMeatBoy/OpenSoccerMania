// Asset-free regression; run only through tools/run_offscreen.py.
#include <windows.h>
#include <d3d8.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
static WNDPROC downstream=nullptr;
static unsigned depth=0,shim_calls=0,legacy_calls=0;
static bool recursed=false;
static void check(bool value,const char* text){if(!value){std::fprintf(stderr,"FAIL: %s\n",text);std::exit(1);}}
static LRESULT CALLBACK legacy(HWND window,UINT message,WPARAM w,LPARAM l) {
    if(message==WM_APP+7){++legacy_calls;return 0x5a51;}
    return DefWindowProcA(window,message,w,l);
}
static LRESULT CALLBACK forwarder(HWND window,UINT message,WPARAM w,LPARAM l) {
    ++shim_calls;
    if(++depth>8){recursed=true;--depth;return DefWindowProcA(window,message,w,l);}
    const auto result=CallWindowProcA(downstream,window,message,w,l);--depth;return result;
}
int main(int argc,char** argv) {
    const bool original_ratio=argc>1&&std::strcmp(argv[1],"--original-aspect")==0;
    const bool adaptive=original_ratio||(argc>1&&std::strcmp(argv[1],"--adaptive")==0);
    check(WritePrivateProfileStringA("Graphics","AutoWindowAspect",adaptive?"1":"0",".\\opensoccermania.ini")!=0,"Configure adaptive resize variant");
    check(WritePrivateProfileStringA("Graphics","ExperimentalHorPlus",original_ratio?"0":"1",".\\opensoccermania.ini")!=0,"Configure original-ratio variant");
    WNDCLASSA cls={};cls.hInstance=GetModuleHandleA(nullptr);cls.lpfnWndProc=legacy;cls.lpszClassName="OSMLayeredWindowRegression";RegisterClassA(&cls);
    HWND window=CreateWindowA(cls.lpszClassName,"OpenSoccerMania window chain test",WS_OVERLAPPEDWINDOW,0,0,640,480,nullptr,nullptr,cls.hInstance,nullptr);
    HMODULE module=LoadLibraryA(".\\d3d8.dll");check(module!=nullptr,"Load bridge");
    auto factory=reinterpret_cast<IDirect3D8* (WINAPI *)(UINT)>(GetProcAddress(module,"Direct3DCreate8"));check(factory!=nullptr,"D3D8 factory");
    IDirect3D8* api=factory(D3D_SDK_VERSION);check(api!=nullptr,"Create API");
    D3DPRESENT_PARAMETERS params={};params.BackBufferWidth=640;params.BackBufferHeight=480;params.BackBufferFormat=D3DFMT_X8R8G8B8;params.BackBufferCount=1;params.SwapEffect=D3DSWAPEFFECT_DISCARD;params.hDeviceWindow=window;params.Windowed=TRUE;
    IDirect3DDevice8 *first=nullptr,*second=nullptr;
    check(SUCCEEDED(api->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,&params,&first)),"Create first device");
    check(SendMessageA(window,WM_GETICON,ICON_SMALL,0)!=0&&SendMessageA(window,WM_GETICON,ICON_BIG,0)!=0,"Game window has branded small and large icons");
    downstream=reinterpret_cast<WNDPROC>(SetWindowLongPtrA(window,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(forwarder)));
    check(downstream!=nullptr,"Layer a forwarding window procedure");
    const auto caption=[&]{return (GetWindowLongA(window,GWL_STYLE)&WS_CAPTION)!=0;};
    check(caption(),"Initial windowed caption");
    SendMessageA(window,WM_KEYDOWN,VK_F11,1);SendMessageA(window,WM_KEYUP,VK_F11,0xc0000001);
    check(!caption()&&!recursed,"F11 reaches inner bridge and switches to borderless");
    check(SendMessageA(window,WM_APP+7,0,0)==0x5a51&&legacy_calls==1&&!recursed,"Appearance update must not capture its forwarding window layer as the downstream");
    check(shim_calls>0,"Installed forwarding layer received messages");
    SendMessageA(window,WM_KEYDOWN,VK_F11,1);SendMessageA(window,WM_KEYUP,VK_F11,0xc0000001);
    check(caption()&&!recursed,"F11 returns to windowed without changing the forwarding chain");
    check(SUCCEEDED(api->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,&params,&second)),"Create second device");
    check(!recursed,"Device recreation must not capture a forwarding window layer as its downstream");
    check(SendMessageA(window,WM_APP+7,0,0)==0x5a51&&legacy_calls==2,"Recreated device still forwards to the true original procedure once");
    if(adaptive) {
        RECT frame={0,0,640,480};
        check(AdjustWindowRectEx(&frame,GetWindowLongA(window,GWL_STYLE),FALSE,GetWindowLongA(window,GWL_EXSTYLE)),"Calculate private frame");
        check(SetWindowPos(window,nullptr,0,0,frame.right-frame.left,frame.bottom-frame.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE),"Resize private window");
        for(unsigned i=0;i<8;++i) {
            check(SUCCEEDED(second->Present(nullptr,nullptr,nullptr,nullptr)),"Present adaptive frame");
            MSG message={};while(PeekMessageA(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageA(&message);}
        }
        RECT client={};check(GetClientRect(window,&client),"Read resized client");
        std::printf("adaptive client=%ldx%ld\n",client.right,client.bottom);
        check(client.right==640&&client.bottom==480,"Adaptive source rectangle must not resize the user's window");
    }
    second->Release();first->Release();api->Release();DestroyWindow(window);
    std::puts("PASS: layered window chain, repeated CreateDevice, original-message dispatch and F11 toggles");
}
