// Asset-free reset regression; run only through tools/run_offscreen.py.
#include <windows.h>
#include <d3d8.h>
#include <cstdio>
#include <cstdlib>
static void check(bool ok,const char* text){if(!ok){std::fprintf(stderr,"FAIL: %s\n",text);std::exit(1);}}
static LRESULT CALLBACK procedure(HWND w,UINT m,WPARAM a,LPARAM b){return DefWindowProcA(w,m,a,b);}
int main(){
    WNDCLASSA cls={};cls.hInstance=GetModuleHandleA(nullptr);cls.lpfnWndProc=procedure;cls.lpszClassName="OSMResetRegression";RegisterClassA(&cls);
    HWND window=CreateWindowA(cls.lpszClassName,"Private reset regression",WS_OVERLAPPEDWINDOW,0,0,640,480,nullptr,nullptr,cls.hInstance,nullptr);
    HMODULE module=LoadLibraryA(".\\d3d8.dll");check(module!=nullptr,"Load production bridge");
    auto factory=reinterpret_cast<IDirect3D8* (WINAPI *)(UINT)>(GetProcAddress(module,"Direct3DCreate8"));check(factory!=nullptr,"Factory");
    IDirect3D8* api=factory(D3D_SDK_VERSION);check(api!=nullptr,"API");
    D3DPRESENT_PARAMETERS params={};params.BackBufferWidth=640;params.BackBufferHeight=480;params.BackBufferFormat=D3DFMT_X8R8G8B8;params.BackBufferCount=1;params.SwapEffect=D3DSWAPEFFECT_DISCARD;params.hDeviceWindow=window;params.Windowed=TRUE;
    IDirect3DDevice8* device=nullptr;check(SUCCEEDED(api->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,&params,&device)),"Device");
    const auto logical_target=[&](unsigned width,unsigned height){
        IDirect3DSurface8* target=nullptr;check(SUCCEEDED(device->GetRenderTarget(&target)),"Get current target without GetBackBuffer workaround");
        D3DSURFACE_DESC desc={};check(SUCCEEDED(target->GetDesc(&desc)),"Target description");target->Release();
        std::printf("logical target=%ux%u expected=%ux%u\n",desc.Width,desc.Height,width,height);
        check(desc.Width==width&&desc.Height==height,"Reset must restore logical backbuffer identity before the game queries its current target");
    };
    logical_target(640,480);
    const unsigned sizes[][2]={{640,480},{1280,720},{1024,768},{640,480}};
    for(const auto& size:sizes){
        params.BackBufferWidth=size[0];params.BackBufferHeight=size[1];
        check(SUCCEEDED(device->Reset(&params)),"Reset device");
        logical_target(size[0],size[1]);
        D3DVIEWPORT8 viewport={};check(SUCCEEDED(device->GetViewport(&viewport)),"Viewport after reset");
        check(viewport.X==0&&viewport.Y==0&&viewport.Width==size[0]&&viewport.Height==size[1],"Reset restores full logical viewport");
        check(SUCCEEDED(device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff123456,1,0)),"Clear after reset");
        check(SUCCEEDED(device->Present(nullptr,nullptr,nullptr,nullptr)),"Present after reset");
    }
    check(device->Release()==0,"Complete device release");api->Release();DestroyWindow(window);
    std::puts("PASS: reset preserves logical target identity and presentation");
}
