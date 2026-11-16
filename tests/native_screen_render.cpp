// Synthetic render regression: no game assets or executable are required.
#include <windows.h>
#include <d3d8.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

static void check(bool value,const char* message){if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
static LRESULT CALLBACK window_proc(HWND window,UINT message,WPARAM w,LPARAM l){return DefWindowProcA(window,message,w,l);}
struct Vertex {float x,y,z,rhw;DWORD diffuse,specular;float u,v;};

int main(int argc,char** argv) {
    const bool raw=argc>1&&std::strcmp(argv[1],"--raw-backend")==0;
    HINSTANCE instance=GetModuleHandleA(nullptr);
    WNDCLASSA cls={};cls.lpfnWndProc=window_proc;cls.hInstance=instance;cls.lpszClassName="OSMRenderRegression";
    RegisterClassA(&cls);
    HWND window=CreateWindowA(cls.lpszClassName,"OpenSoccerMania layer test",WS_OVERLAPPEDWINDOW,0,0,640,480,nullptr,nullptr,instance,nullptr);
    HMODULE module=LoadLibraryA(raw?".\\d3d8_backend.dll":".\\d3d8.dll");check(module!=nullptr,"Load render bridge/backend");
    auto factory=reinterpret_cast<IDirect3D8* (WINAPI *)(UINT)>(GetProcAddress(module,"Direct3DCreate8"));check(factory!=nullptr,"D3D8 export");
    IDirect3D8* api=factory(D3D_SDK_VERSION);check(api!=nullptr,"Create D3D8");
    D3DPRESENT_PARAMETERS params={};params.BackBufferWidth=raw?1920:640;params.BackBufferHeight=raw?1080:480;params.BackBufferFormat=D3DFMT_X8R8G8B8;
    params.BackBufferCount=1;params.SwapEffect=D3DSWAPEFFECT_DISCARD;params.hDeviceWindow=window;params.Windowed=TRUE;
    IDirect3DDevice8* device=nullptr;
    check(SUCCEEDED(api->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,&params,&device)),"Create windowed device");
    constexpr DWORD color=0xff2a7bd2;
    Vertex data[7]={};
    data[3]={0,0,1.0015f,.00002f,color,0,0,0};data[4]={640,0,1.0015f,.00002f,color,0,0,0};
    data[5]={0,480,1.0015f,.00002f,color,0,0,0};data[6]={640,480,1.0015f,.00002f,color,0,0,0};
    IDirect3DVertexBuffer8* vb=nullptr;IDirect3DIndexBuffer8* ib=nullptr;BYTE* bytes=nullptr;
    check(SUCCEEDED(device->CreateVertexBuffer(sizeof(data),D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,0x1c4,D3DPOOL_DEFAULT,&vb)),"Create dynamic screen vertex buffer");
    check(SUCCEEDED(vb->Lock(0,0,&bytes,D3DLOCK_DISCARD)),"Write vertices");std::memcpy(bytes,data,sizeof(data));vb->Unlock();
    const WORD indices[]={0,0,0,2,3,4,4,3,5};
    check(SUCCEEDED(device->CreateIndexBuffer(sizeof(indices),D3DUSAGE_WRITEONLY,D3DFMT_INDEX16,D3DPOOL_MANAGED,&ib)),"Create index buffer");
    check(SUCCEEDED(ib->Lock(0,0,&bytes,0)),"Write indices");std::memcpy(bytes,indices,sizeof(indices));ib->Unlock();
    device->SetVertexShader(0x1c4);device->SetPixelShader(0);
    device->SetRenderState(D3DRS_ZENABLE,FALSE);device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
    device->SetRenderState(D3DRS_LIGHTING,FALSE);device->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
    device->SetRenderState(D3DRS_FOGENABLE,argc>1&&std::strcmp(argv[1],"--no-fog")==0?FALSE:TRUE);device->SetRenderState(D3DRS_FOGVERTEXMODE,D3DFOG_LINEAR);
    device->SetTexture(0,nullptr);device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);
    device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
    IDirect3DTexture8* pattern=nullptr;
    for(unsigned pass=0;pass<3;++pass) {
        if(pass==2) {
            check(SUCCEEDED(device->CreateTexture(2,1,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&pattern)),"Create repeating sky-pattern texture");
            D3DLOCKED_RECT tex={};check(SUCCEEDED(pattern->LockRect(0,&tex,nullptr,0)),"Write sky pattern");
            const DWORD pixels[]={color,0xffd22a7b};std::memcpy(tex.pBits,pixels,sizeof(pixels));pattern->UnlockRect(0);
            device->SetTexture(0,pattern);device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);
            device->SetTextureStageState(0,D3DTSS_MINFILTER,D3DTEXF_POINT);device->SetTextureStageState(0,D3DTSS_MAGFILTER,D3DTEXF_POINT);
            device->SetTextureStageState(0,D3DTSS_ADDRESSU,D3DTADDRESS_WRAP);device->SetTextureStageState(0,D3DTSS_ADDRESSV,D3DTADDRESS_WRAP);
            data[3].u=0;data[4].u=4;data[5].u=0;data[6].u=4;
            check(SUCCEEDED(vb->Lock(0,0,&bytes,D3DLOCK_DISCARD)),"Update textured screen layer");std::memcpy(bytes,data,sizeof(data));vb->Unlock();
        }
        device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000,1,0);
        device->SetStreamSource(0,vb,sizeof(Vertex));device->SetIndices(ib,1);
        if(pass==1){DWORD token=0;check(SUCCEEDED(device->CreateStateBlock(D3DSBT_ALL,&token)),"Capture original shader state");device->SetVertexShader(D3DFVF_XYZ|D3DFVF_DIFFUSE);check(SUCCEEDED(device->ApplyStateBlock(token)),"Restore shader through state block");device->DeleteStateBlock(token);}
        check(SUCCEEDED(device->BeginScene()),"Begin scene");
        HRESULT drawn=pass==1?device->DrawPrimitive(D3DPT_TRIANGLESTRIP,3,2):device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,2,4,3,2);
        check(SUCCEEDED(drawn),"Draw projected screen layer");
        DWORD fog=0;device->GetRenderState(D3DRS_FOGENABLE,&fog);
        check(fog==(argc>1&&std::strcmp(argv[1],"--no-fog")==0?FALSE:TRUE),"Restore scene fog after background draw");
        IDirect3DVertexBuffer8* bound=nullptr;UINT stride=0;
        device->GetStreamSource(0,&bound,&stride);check(bound==vb&&stride==sizeof(Vertex),"Restore vertex binding");if(bound)bound->Release();
        IDirect3DIndexBuffer8* bound_indices=nullptr;UINT base=0;
        device->GetIndices(&bound_indices,&base);check(bound_indices==ib&&base==1,"Restore index binding");if(bound_indices)bound_indices->Release();
        device->EndScene();
        IDirect3DSurface8 *back=nullptr,*copy=nullptr;device->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&back);
        D3DSURFACE_DESC desc={};back->GetDesc(&desc);check(desc.Width==(raw?1920u:640u)&&desc.Height==(raw?1080u:480u),"Preserve logical backbuffer description");
        check(SUCCEEDED(device->CreateImageSurface(1920,1080,desc.Format,&copy)),"Create physical readback");
        check(SUCCEEDED(device->CopyRects(back,nullptr,0,copy,nullptr)),"Copy physical render target");
        D3DLOCKED_RECT lock={};check(SUCCEEDED(copy->LockRect(&lock,nullptr,D3DLOCK_READONLY)),"Read physical target");
        for(unsigned row=0;row<3;++row)for(unsigned i=0;i<(pass==2?8u:3u);++i) {
            const unsigned y=raw?(row==0?10:row==1?240:460):(row==0?10:row==1?540:1060);
            const unsigned x=pass==2?(raw?40+i*80:120+i*240):raw?(i==0?10:i==1?320:630):i==0?10:i==1?960:1900;
            const DWORD expected=pass==2&&(i&1)?0xffd22a7b:color;
            DWORD pixel=0;std::memcpy(&pixel,static_cast<const BYTE*>(lock.pBits)+y*lock.Pitch+x*4,4);
            if((pixel&0xffffff)!=(expected&0xffffff))std::fprintf(stderr,"pixel[%u,%u]=%08lx expected=%08lx\n",x,y,pixel,expected);
            check((pixel&0xffffff)==(expected&0xffffff),pass==2?"Preserve repeated world texture UVs":"Screen layer covers full physical output");
        }
        copy->UnlockRect();copy->Release();back->Release();
        std::printf("PASS: %s layer covers 1920x1080, original bindings preserved\n",pass==2?"repeating world texture":pass==1?"non-indexed":"indexed/base-offset");
    }
    if(argc>1&&std::strcmp(argv[1],"--benchmark")==0) {
        D3DVIEWPORT8 tiny={0,0,2,2,0,1};device->SetViewport(&tiny);
        device->SetVertexShader(0x1c4);device->SetStreamSource(0,vb,sizeof(Vertex));device->SetIndices(ib,1);
        LARGE_INTEGER frequency,start,end;QueryPerformanceFrequency(&frequency);LONGLONG total=0;
        constexpr unsigned frames=240,draws=128,warmup=20;
        for(unsigned frame=0;frame<frames+warmup;++frame) {
            device->BeginScene();QueryPerformanceCounter(&start);
            for(unsigned draw=0;draw<draws;++draw)check(SUCCEEDED(device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,2,4,3,2)),"Benchmark draw");
            QueryPerformanceCounter(&end);device->EndScene();
            if(frame>=warmup)total+=end.QuadPart-start.QuadPart;
            check(SUCCEEDED(device->Present(nullptr,nullptr,nullptr,nullptr)),"Benchmark present");
        }
        const double milliseconds=1000.*total/frequency.QuadPart;
        std::printf("BENCHMARK frames=%u draws=%u draw_cpu_ms=%.6f cpu_us_per_draw=%.6f\n",frames,frames*draws,milliseconds,1000.*milliseconds/(frames*draws));
    }
    device->SetTexture(0,nullptr);if(pattern)pattern->Release();
    device->SetStreamSource(0,nullptr,0);device->SetIndices(nullptr,0);vb->Release();ib->Release();
    device->Release();api->Release();DestroyWindow(window);
}
