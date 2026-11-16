// Asset-free production-bridge regression. Run on an isolated Win32 desktop.
// The unused image padding hosts the inspected producer signatures/data at the
// original addresses; no game code or assets are loaded or executed.
#include <windows.h>
#include <d3d8.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
static unsigned char image_padding[0x300000];
struct Vertex {float x,y,z,rhw;DWORD color;};
static void check(bool value,const char* text){if(!value){std::fprintf(stderr,"FAIL: %s\n",text);std::exit(1);}}
static LRESULT CALLBACK procedure(HWND w,UINT m,WPARAM a,LPARAM b){return DefWindowProcA(w,m,a,b);}
static void producer_fixture() {
    check(GetModuleHandleA(nullptr)==reinterpret_cast<HMODULE>(0x400000),"Fixed image base");
    const auto begin=reinterpret_cast<std::uintptr_t>(image_padding),end=begin+sizeof(image_padding);
    check(begin<=0x4c9100&&end>=0x65ba70,"Producer fixture addresses belong to unused test image padding");
    const BYTE entry[]={0xa1,0x00,0xb8,0x65,0x00,0x85,0xc0,0x0f,0x84,0x1d,0x01,0x00,0x00};
    const BYTE draw[]={0x6a,0x44,0x50,0xff,0x91,0x30,0x01,0x00,0x00,0xa1,0x28,0x6c,0x61,0x00,0x6a,0x14,0x68,0x20,0xba,0x65,0x00,0x6a,0x02,0x8b,0x10,0x6a,0x05,0x50,0xff,0x92,0x20,0x01,0x00,0x00};
    std::memcpy(reinterpret_cast<void*>(0x4c9100),entry,sizeof(entry));
    std::memcpy(reinterpret_cast<void*>(0x4c91ee),draw,sizeof(draw));
}
static DWORD sample(IDirect3DDevice8* device,const char* capture,unsigned x,unsigned y,IDirect3DSurface8* source=nullptr) {
    IDirect3DSurface8 *back=nullptr,*image=nullptr;D3DSURFACE_DESC desc={};
    if(source){back=source;back->AddRef();}else check(SUCCEEDED(device->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&back)),"Get backbuffer");
    check(SUCCEEDED(back->GetDesc(&desc)),"Get output pixel format");
    check(SUCCEEDED(device->CreateImageSurface(1920,1080,desc.Format,&image)),"Create physical readback surface");
    check(SUCCEEDED(device->CopyRects(back,nullptr,0,image,nullptr)),"Copy physical output");
    D3DLOCKED_RECT lock={};check(SUCCEEDED(image->LockRect(&lock,nullptr,D3DLOCK_READONLY)),"Read output pixels");
    auto pixel=[&](unsigned px,unsigned py) {
        const BYTE* row=static_cast<const BYTE*>(lock.pBits)+py*lock.Pitch;
        if(desc.Format==D3DFMT_R5G6B5){const WORD p=reinterpret_cast<const WORD*>(row)[px];return DWORD(((p>>11)*255/31)<<16|(((p>>5)&63)*255/63)<<8|(p&31)*255/31);}
        check(desc.Format==D3DFMT_X8R8G8B8||desc.Format==D3DFMT_A8R8G8B8,"Supported output capture format");
        return reinterpret_cast<const DWORD*>(row)[px]&0xffffff;
    };
    const DWORD result=pixel(x,y);
    if(capture){FILE* file=std::fopen(capture,"wb");check(file!=nullptr,"Create private output capture");std::fprintf(file,"P6\n1920 1080\n255\n");for(unsigned py=0;py<1080;++py)for(unsigned px=0;px<1920;++px){const DWORD p=pixel(px,py);const BYTE rgb[]={BYTE(p>>16),BYTE(p>>8),BYTE(p)};std::fwrite(rgb,3,1,file);}std::fclose(file);}
    image->UnlockRect();image->Release();back->Release();return result;
}
int main() {
    producer_fixture();
    WNDCLASSA cls={};cls.hInstance=GetModuleHandleA(nullptr);cls.lpfnWndProc=procedure;cls.lpszClassName="OSMWorldStencilRegression";RegisterClassA(&cls);
    HWND window=CreateWindowA(cls.lpszClassName,"OpenSoccerMania world effect test",WS_OVERLAPPEDWINDOW,0,0,640,480,nullptr,nullptr,cls.hInstance,nullptr);
    HMODULE module=LoadLibraryA(".\\d3d8.dll");check(module!=nullptr,"Load production bridge");
    auto factory=reinterpret_cast<IDirect3D8* (WINAPI *)(UINT)>(GetProcAddress(module,"Direct3DCreate8"));check(factory!=nullptr,"D3D8 factory");
    IDirect3D8* api=factory(D3D_SDK_VERSION);check(api!=nullptr,"Create API");
    D3DPRESENT_PARAMETERS params={};params.BackBufferWidth=640;params.BackBufferHeight=480;params.BackBufferFormat=D3DFMT_X8R8G8B8;params.BackBufferCount=1;params.SwapEffect=D3DSWAPEFFECT_DISCARD;params.hDeviceWindow=window;params.Windowed=TRUE;params.EnableAutoDepthStencil=TRUE;params.AutoDepthStencilFormat=D3DFMT_D24S8;
    IDirect3DDevice8* device=nullptr;check(SUCCEEDED(api->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,&params,&device)),"Create device");
    const Vertex quad[]={{0,0,0,1,0x40000000},{639,0,0,1,0x40000000},{0,479,0,1,0x40000000},{639,479,0,1,0x40000000}};
    auto* world=reinterpret_cast<Vertex*>(0x65ba20);std::memcpy(world,quad,sizeof(quad));
    device->SetVertexShader(0x44);device->SetTexture(0,nullptr);
    device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG2);device->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG2);device->SetTextureStageState(0,D3DTSS_ALPHAARG2,D3DTA_DIFFUSE);device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
    device->SetRenderState(D3DRS_ZENABLE,FALSE);device->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);device->SetRenderState(D3DRS_FOGENABLE,FALSE);device->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);device->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
    device->SetRenderState(D3DRS_STENCILENABLE,TRUE);device->SetRenderState(D3DRS_STENCILFUNC,D3DCMP_EQUAL);device->SetRenderState(D3DRS_STENCILREF,1);device->SetRenderState(D3DRS_STENCILMASK,~0u);device->SetRenderState(D3DRS_STENCILWRITEMASK,0);device->SetRenderState(D3DRS_STENCILPASS,D3DSTENCILOP_KEEP);
    auto draw=[&](const Vertex* data){check(SUCCEEDED(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,0xffffffff,1,1)),"Clear test frame");check(SUCCEEDED(device->BeginScene()),"Begin scene");check(SUCCEEDED(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,data,sizeof(Vertex))),"Draw test composite");check(SUCCEEDED(device->EndScene()),"End scene");};
    draw(world);const DWORD world_edge=sample(device,"world-stencil.ppm",100,540),world_center=sample(device,nullptr,960,540);
    std::printf("world edge=%06lx center=%06lx\n",world_edge,world_center);
    check((world_edge&255)<200&&(world_center&255)<200,"World stencil composite must shade beyond the original 4:3 center");
    const DWORD top_left=sample(device,nullptr,0,0),bottom_right=sample(device,nullptr,1919,1079);
    std::printf("world corners=%06lx,%06lx\n",top_left,bottom_right);
    check((top_left&255)<200&&(bottom_right&255)<200,"Known full-screen resolve covers first and last output pixels");
    draw(quad);const DWORD hud_edge=sample(device,"ordinary-hud.ppm",100,540),hud_center=sample(device,nullptr,960,540);
    std::printf("HUD edge=%06lx center=%06lx\n",hud_edge,hud_center);
    check((hud_edge&255)>240&&(hud_center&255)<200,"Ordinary same-depth HUD must retain centered proportions");
    DWORD stencil=0;device->GetRenderState(D3DRS_STENCILENABLE,&stencil);check(stencil==TRUE,"Composite correction preserves stencil state");
    IDirect3DSurface8 *back=nullptr,*back_depth=nullptr,*target=nullptr,*target_depth=nullptr;
    device->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&back);device->GetDepthStencilSurface(&back_depth);
    check(SUCCEEDED(device->CreateRenderTarget(1920,1080,D3DFMT_X8R8G8B8,D3DMULTISAMPLE_NONE,FALSE,&target)),"Create offscreen target");
    check(SUCCEEDED(device->CreateDepthStencilSurface(1920,1080,D3DFMT_D24S8,D3DMULTISAMPLE_NONE,&target_depth)),"Create offscreen stencil");
    check(SUCCEEDED(device->SetRenderTarget(target,target_depth)),"Bind offscreen target");
    D3DVIEWPORT8 viewport={0,0,1920,1080,0,1};device->SetViewport(&viewport);
    draw(world);const DWORD rt_right=sample(device,nullptr,639,100,target),rt_bottom=sample(device,nullptr,100,479,target);
    draw(quad);check(sample(device,nullptr,639,100,target)==rt_right&&sample(device,nullptr,100,479,target)==rt_bottom,"Render-texture path must not receive backbuffer edge expansion");
    std::puts("PASS: render-texture coordinates remain unchanged");
    check(SUCCEEDED(device->SetRenderTarget(back,back_depth)),"Restore backbuffer");
    target->Release();target_depth->Release();back->Release();back_depth->Release();
    device->Release();api->Release();DestroyWindow(window);std::puts("PASS: full-width world stencil resolve and unchanged centered HUD");
}
