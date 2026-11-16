// Asset-free GPU regression. Run only through tools/run_offscreen.py.
#define CINTERFACE
#define COBJMACROS
#include "../native/title_background.h"
#include <cstdlib>
static void check(bool ok,const char* text){if(!ok){std::fprintf(stderr,"FAIL: %s\n",text);std::exit(1);}}
static LRESULT CALLBACK procedure(HWND w,UINT m,WPARAM a,LPARAM b){return DefWindowProcA(w,m,a,b);}
int main(){
    WNDCLASSA cls={};cls.hInstance=GetModuleHandleA(nullptr);cls.lpfnWndProc=procedure;cls.lpszClassName="OSMTitleRegression";RegisterClassA(&cls);
    HWND window=CreateWindowA(cls.lpszClassName,"Private title regression",WS_OVERLAPPEDWINDOW,0,0,640,480,nullptr,nullptr,cls.hInstance,nullptr);
    HMODULE module=LoadLibraryA(".\\d3d8_backend.dll");check(module!=nullptr,"Load real backend");
    auto factory=reinterpret_cast<IDirect3D8* (WINAPI *)(UINT)>(GetProcAddress(module,"Direct3DCreate8"));check(factory!=nullptr,"Factory");
    IDirect3D8* api=factory(D3D_SDK_VERSION);check(api!=nullptr,"API");
    D3DPRESENT_PARAMETERS params={};params.BackBufferWidth=1920;params.BackBufferHeight=1080;params.BackBufferFormat=D3DFMT_X8R8G8B8;params.BackBufferCount=1;params.SwapEffect=D3DSWAPEFFECT_DISCARD;params.hDeviceWindow=window;params.Windowed=TRUE;
    IDirect3DDevice8* device=nullptr;check(SUCCEEDED(IDirect3D8_CreateDevice(api,0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,&params,&device)),"Device");
    osm::title_background.configure(".\\title-test.ini");osm::title_background.set_device(device);
    D3DVIEWPORT8 before={13,17,640,480,0.1f,0.9f};
    IDirect3DDevice8_SetRenderState(device,D3DRS_FOGENABLE,TRUE);IDirect3DDevice8_SetRenderState(device,D3DRS_ALPHABLENDENABLE,TRUE);
    IDirect3DVertexBuffer8* vb=nullptr;check(SUCCEEDED(IDirect3DDevice8_CreateVertexBuffer(device,128,0,D3DFVF_XYZ,D3DPOOL_MANAGED,&vb)),"Stream buffer");IDirect3DDevice8_SetStreamSource(device,0,vb,12);
    check(SUCCEEDED(IDirect3DDevice8_Clear(device,0,nullptr,D3DCLEAR_TARGET,0xffb02040,1,0)),"Clear");IDirect3DDevice8_SetViewport(device,&before);
    check(SUCCEEDED(IDirect3DDevice8_BeginScene(device)),"Begin scene");check(osm::title_background.draw(),"Draw sidebar helper");check(!osm::title_background_drawing,"Scoped draw flag reset");
    D3DVIEWPORT8 after={};IDirect3DDevice8_GetViewport(device,&after);check(!std::memcmp(&before,&after,sizeof(before)),"Viewport restored");
    DWORD value=0;IDirect3DDevice8_GetRenderState(device,D3DRS_FOGENABLE,&value);check(value==TRUE,"Fog restored");IDirect3DDevice8_GetRenderState(device,D3DRS_ALPHABLENDENABLE,&value);check(value==TRUE,"Blend restored");
    IDirect3DVertexBuffer8* stream=nullptr;UINT stride=0;IDirect3DDevice8_GetStreamSource(device,0,&stream,&stride);check(stream==vb&&stride==12,"UP stream restored");if(stream)IDirect3DVertexBuffer8_Release(stream);
    check(SUCCEEDED(IDirect3DDevice8_EndScene(device)),"End scene");
    IDirect3DSurface8* back=nullptr;check(SUCCEEDED(IDirect3DDevice8_GetRenderTarget(device,&back)),"Target");D3DSURFACE_DESC desc={};IDirect3DSurface8_GetDesc(back,&desc);
    IDirect3DSurface8* copy=nullptr;check(SUCCEEDED(IDirect3DDevice8_CreateImageSurface(device,1920,1080,desc.Format,&copy)),"Readback surface");check(SUCCEEDED(IDirect3DDevice8_CopyRects(device,back,nullptr,0,copy,nullptr)),"Readback copy");
    D3DLOCKED_RECT lock={};check(SUCCEEDED(IDirect3DSurface8_LockRect(copy,&lock,nullptr,D3DLOCK_READONLY)),"Readback lock");
    auto pixel=[&](unsigned x,unsigned y){return *reinterpret_cast<const DWORD*>(static_cast<const BYTE*>(lock.pBits)+y*lock.Pitch+x*4)&0xffffff;};
    check(pixel(10,540)==0x123456&&pixel(1900,540)==0x123456,"Both sidebar regions reached GPU");
    check(pixel(240,540)==0xb02040&&pixel(960,540)==0xb02040&&pixel(1679,540)==0xb02040,"Entire 4:3 center preserved");
    check(pixel(239,540)==0x123456&&pixel(1680,540)==0x123456,"No center intrusion at exact joins");
    IDirect3DSurface8_UnlockRect(copy);IDirect3DSurface8_Release(copy);IDirect3DSurface8_Release(back);osm::title_background.release();IDirect3DVertexBuffer8_Release(vb);IDirect3DDevice8_Release(device);IDirect3D8_Release(api);DestroyWindow(window);
    std::puts("PASS: sidebar GPU colors, exact center preservation, viewport/fog/blend/stream state");return 0;
}
