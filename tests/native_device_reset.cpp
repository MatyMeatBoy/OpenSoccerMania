// Asset-free reset regression; run only through tools/run_offscreen.py.
#include <windows.h>
#include <d3d8.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
static void check(bool ok,const char* text){if(!ok){std::fprintf(stderr,"FAIL: %s\n",text);std::exit(1);}}
static LRESULT CALLBACK procedure(HWND w,UINT m,WPARAM a,LPARAM b){return DefWindowProcA(w,m,a,b);}
int main(int argc,char** argv){
    const bool adaptive=argc>1&&std::strcmp(argv[1],"--adaptive")==0;
    check(WritePrivateProfileStringA("Graphics","AutoWindowAspect",adaptive?"1":"0",".\\opensoccermania.ini")!=0,"Configure adaptive reset variant");
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
    check(device->Reset(nullptr)==D3DERR_INVALIDCALL,"Null reset parameters are rejected");
    logical_target(640,480);
    const auto resize_client=[&]{
        RECT rect={0,0,640,480};
        check(AdjustWindowRectEx(&rect,GetWindowLongA(window,GWL_STYLE),FALSE,GetWindowLongA(window,GWL_EXSTYLE)),"Calculate resized window frame");
        check(SetWindowPos(window,nullptr,0,0,rect.right-rect.left,rect.bottom-rect.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE),"Resize private client");
        check(SUCCEEDED(device->Present(nullptr,nullptr,nullptr,nullptr)),"Apply adaptive presentation");
    };
    const auto adaptive_pixels=[&](unsigned width,unsigned height){
        check(SUCCEEDED(device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000,1,0)),"Clear adaptive output");
        D3DRECT quarter={0,0,static_cast<LONG>(width/4),static_cast<LONG>(height)};
        check(SUCCEEDED(device->Clear(1,&quarter,D3DCLEAR_TARGET,0xffcc3311,1,0)),"Draw logical left quarter");
        IDirect3DSurface8 *back=nullptr,*copy=nullptr;
        check(SUCCEEDED(device->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&back)),"Adaptive backbuffer");
        check(SUCCEEDED(device->CreateImageSurface(1920,1080,D3DFMT_X8R8G8B8,&copy)),"Adaptive readback surface");
        check(SUCCEEDED(device->CopyRects(back,nullptr,0,copy,nullptr)),"Copy adaptive pixels");
        D3DLOCKED_RECT pixels={};check(SUCCEEDED(copy->LockRect(&pixels,nullptr,D3DLOCK_READONLY)),"Read adaptive pixels");
        const auto* row=reinterpret_cast<const DWORD*>(static_cast<const BYTE*>(pixels.pBits)+540*pixels.Pitch);
        check((row[100]&0xffffff)==0,"Adaptive left margin stays outside logical drawing after reset");
        check((row[550]&0xffffff)==0xcc3311,"Adaptive logical quarter retains its fitted width after reset");
        copy->UnlockRect();copy->Release();back->Release();
    };
    if(adaptive){resize_client();adaptive_pixels(640,480);}
    const unsigned sizes[][2]={{640,480},{1280,720},{1024,768},{640,480}};
    for(const auto& size:sizes){
        // Invalid color-buffer formats must fail without preventing a later
        // valid reset. Do not issue draw/state calls while the device is lost.
        D3DPRESENT_PARAMETERS invalid=params;
        invalid.BackBufferWidth=800;invalid.BackBufferHeight=600;
        invalid.BackBufferFormat=D3DFMT_INDEX16;
        check(FAILED(device->Reset(&invalid)),"Reject invalid reset before retry");
        params.BackBufferWidth=size[0];params.BackBufferHeight=size[1];
        check(SUCCEEDED(device->Reset(&params)),"Reset device");
        if(adaptive){resize_client();adaptive_pixels(size[0],size[1]);}
        logical_target(size[0],size[1]);
        D3DVIEWPORT8 viewport={};check(SUCCEEDED(device->GetViewport(&viewport)),"Viewport after reset");
        check(viewport.X==0&&viewport.Y==0&&viewport.Width==size[0]&&viewport.Height==size[1],"Reset restores full logical viewport");
        check(SUCCEEDED(device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff123456,1,0)),"Clear after reset");
        IDirect3DSurface8 *back=nullptr,*copy=nullptr;
        check(SUCCEEDED(device->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&back)),"Recovered backbuffer");
        check(SUCCEEDED(device->CreateImageSurface(1920,1080,D3DFMT_X8R8G8B8,&copy)),"Recovered output readback");
        check(SUCCEEDED(device->CopyRects(back,nullptr,0,copy,nullptr)),"Copy recovered output");
        D3DLOCKED_RECT pixels={};check(SUCCEEDED(copy->LockRect(&pixels,nullptr,D3DLOCK_READONLY)),"Read recovered output");
        const auto* center=reinterpret_cast<const DWORD*>(static_cast<const BYTE*>(pixels.pBits)+540*pixels.Pitch)+960;
        check((*center&0xffffff)==0x123456,"Recovered device renders the expected physical center pixel");
        copy->UnlockRect();copy->Release();back->Release();
        check(SUCCEEDED(device->Present(nullptr,nullptr,nullptr,nullptr)),"Present after reset");
    }
    check(device->Release()==0,"Complete device release");api->Release();DestroyWindow(window);
    std::puts("PASS: failed reset retries preserve logical target identity and presentation");
}
