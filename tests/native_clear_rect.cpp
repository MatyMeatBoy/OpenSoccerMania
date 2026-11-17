// Asset-free clear-rectangle regression; run only through tools/run_offscreen.py.
#include <windows.h>
#include <d3d8.h>
#include <cstdio>
#include <cstdlib>
static void check(bool ok,const char* text){if(!ok){std::fprintf(stderr,"FAIL: %s\n",text);std::exit(1);}}
static LRESULT CALLBACK procedure(HWND w,UINT m,WPARAM a,LPARAM b){return DefWindowProcA(w,m,a,b);}
int main(int argc,char**){
    const bool original=argc>1;
    if(original)WritePrivateProfileStringA("Graphics","ExperimentalHorPlus","0",".\\opensoccermania.ini");
    WNDCLASSA cls={};cls.hInstance=GetModuleHandleA(nullptr);cls.lpfnWndProc=procedure;cls.lpszClassName="OSMResetRegression";RegisterClassA(&cls);
    HWND window=CreateWindowA(cls.lpszClassName,"Private reset regression",WS_OVERLAPPEDWINDOW,0,0,640,480,nullptr,nullptr,cls.hInstance,nullptr);
    HMODULE module=LoadLibraryA(".\\d3d8.dll");check(module!=nullptr,"Load production bridge");
    auto factory=reinterpret_cast<IDirect3D8* (WINAPI *)(UINT)>(GetProcAddress(module,"Direct3DCreate8"));check(factory!=nullptr,"Factory");
    IDirect3D8* api=factory(D3D_SDK_VERSION);check(api!=nullptr,"API");
    D3DPRESENT_PARAMETERS params={};params.BackBufferWidth=640;params.BackBufferHeight=480;params.BackBufferFormat=D3DFMT_X8R8G8B8;params.BackBufferCount=1;params.SwapEffect=D3DSWAPEFFECT_DISCARD;params.hDeviceWindow=window;params.Windowed=TRUE;params.EnableAutoDepthStencil=TRUE;params.AutoDepthStencilFormat=D3DFMT_D24S8;
    IDirect3DDevice8* device=nullptr;check(SUCCEEDED(api->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,&params,&device)),"Device");

    auto pixel=[&](unsigned x,unsigned y,IDirect3DSurface8* source=nullptr,unsigned width=1920,unsigned height=1080){
        IDirect3DSurface8 *back=source,*image=nullptr;if(back)back->AddRef();else check(SUCCEEDED(device->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&back)),"Backbuffer");
        check(SUCCEEDED(device->CreateImageSurface(width,height,D3DFMT_X8R8G8B8,&image)),"Readback image");
        check(SUCCEEDED(device->CopyRects(back,nullptr,0,image,nullptr)),"Readback copy");D3DLOCKED_RECT lock={};check(SUCCEEDED(image->LockRect(&lock,nullptr,D3DLOCK_READONLY)),"Readback lock");
        const DWORD result=reinterpret_cast<const DWORD*>(static_cast<const BYTE*>(lock.pBits)+y*lock.Pitch)[x]&0xffffff;image->UnlockRect();image->Release();back->Release();return result;
    };
    const unsigned center=original?577:450,left=original?440:250,right=original?720:650;
    const D3DRECT rect={100,100,200,200};
    check(SUCCEEDED(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,0xff0000ff,0,0)),"Full clear");
    check(SUCCEEDED(device->Clear(1,&rect,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,0xffff0000,1,7)),"Logical rectangle clear");
    check(pixel(center,337)==0xff0000,"Logical rectangle must clear its physical center");
    check(pixel(left,337)==0x0000ff&&pixel(right,337)==0x0000ff,"Clear must preserve pixels outside scaled rectangle");
    struct Vertex{float x,y,z,rhw;DWORD color;};
    const Vertex quad[]={{0,0,.5f,1,0xffffffff},{640,0,.5f,1,0xffffffff},{0,480,.5f,1,0xffffffff},{640,480,.5f,1,0xffffffff}};
    device->SetVertexShader(0x44);device->SetTexture(0,nullptr);device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG2);device->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
    device->SetRenderState(D3DRS_ZENABLE,TRUE);device->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);device->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);device->SetRenderState(D3DRS_FOGENABLE,FALSE);device->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
    device->SetRenderState(D3DRS_STENCILENABLE,TRUE);device->SetRenderState(D3DRS_STENCILFUNC,D3DCMP_EQUAL);device->SetRenderState(D3DRS_STENCILREF,7);device->SetRenderState(D3DRS_STENCILMASK,~0u);device->SetRenderState(D3DRS_STENCILPASS,D3DSTENCILOP_KEEP);
    check(SUCCEEDED(device->BeginScene()),"Begin");check(SUCCEEDED(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,quad,sizeof(Vertex))),"Depth/stencil probe");check(SUCCEEDED(device->EndScene()),"End");
    check(pixel(center,337)==0xffffff,"Depth and stencil clear must cover the same physical rectangle");check(pixel(left,337)==0x0000ff,"Outside depth/stencil remains unchanged");
    auto physical_x=[&](float logical){return static_cast<unsigned>((original?240.f:0.f)+logical*(original?2.25f:3.f));};
    const D3DRECT multi[]={{50,300,80,350},{400,250,430,280}};
    check(SUCCEEDED(device->Clear(2,multi,D3DCLEAR_TARGET,0xff00ff00,1,0)),"Multiple rectangle clear");
    check(pixel(physical_x(65),731)==0x00ff00&&pixel(physical_x(415),596)==0x00ff00,"Each clear rectangle maps independently");
    check(SUCCEEDED(device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff0000ff,1,0)),"Reset colors");
    D3DVIEWPORT8 partial={80,80,160,160,0,1};check(SUCCEEDED(device->SetViewport(&partial)),"Partial viewport");
    const D3DRECT full={0,0,640,480};check(SUCCEEDED(device->Clear(1,&full,D3DCLEAR_TARGET,0xffffff00,1,0)),"Clipped clear");
    check(pixel(physical_x(100),225)==0xffff00&&pixel(physical_x(60),225)==0x0000ff,"Backend clips mapped rectangle to physical viewport");
    D3DVIEWPORT8 full_view={0,0,640,480,0,1};device->SetViewport(&full_view);
    IDirect3DSurface8 *back=nullptr,*depth=nullptr,*target=nullptr,*target_depth=nullptr;device->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&back);device->GetDepthStencilSurface(&depth);
    check(SUCCEEDED(device->CreateRenderTarget(640,480,D3DFMT_X8R8G8B8,D3DMULTISAMPLE_NONE,FALSE,&target)),"Offscreen color");check(SUCCEEDED(device->CreateDepthStencilSurface(640,480,D3DFMT_D24S8,D3DMULTISAMPLE_NONE,&target_depth)),"Offscreen depth");check(SUCCEEDED(device->SetRenderTarget(target,target_depth)),"Offscreen target");
    D3DVIEWPORT8 view={0,0,640,480,0,1};device->SetViewport(&view);
    check(SUCCEEDED(device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff0000ff,1,0)),"Offscreen full clear");check(SUCCEEDED(device->Clear(1,&rect,D3DCLEAR_TARGET,0xffff0000,1,0)),"Offscreen rect clear");
    check(pixel(150,150,target,640,480)==0xff0000&&pixel(450,337,target,640,480)==0x0000ff,"Render-texture clears retain original coordinates");
    device->SetRenderTarget(back,depth);target->Release();target_depth->Release();back->Release();depth->Release();device->Release();api->Release();DestroyWindow(window);
    std::puts("PASS: logical color/depth/stencil clear rectangles and unchanged render textures");
}
