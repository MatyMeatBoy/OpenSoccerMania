// Asset-free production-DLL regression; run on a private Win32 desktop.
#include <windows.h>
#include <d3d8.h>
#include <cstdio>
#include <cstdlib>
static void check(bool ok,const char* text){if(!ok){std::fprintf(stderr,"FAIL: %s\n",text);std::exit(1);}}
static LRESULT CALLBACK procedure(HWND w,UINT m,WPARAM a,LPARAM b){return DefWindowProcA(w,m,a,b);}
int main(){
    WNDCLASSA cls={};cls.hInstance=GetModuleHandleA(nullptr);cls.lpfnWndProc=procedure;cls.lpszClassName="OSMShaderResetRegression";RegisterClassA(&cls);
    HWND window=CreateWindowA(cls.lpszClassName,"Private shader reset regression",WS_OVERLAPPEDWINDOW,0,0,640,480,nullptr,nullptr,cls.hInstance,nullptr);
    HMODULE module=LoadLibraryA(".\\d3d8.dll");check(module!=nullptr,"Load production bridge");
    auto factory=reinterpret_cast<IDirect3D8* (WINAPI *)(UINT)>(GetProcAddress(module,"Direct3DCreate8"));check(factory!=nullptr,"Factory");
    IDirect3D8* api=factory(D3D_SDK_VERSION);check(api!=nullptr,"API");
    D3DPRESENT_PARAMETERS params={};params.BackBufferWidth=640;params.BackBufferHeight=480;params.BackBufferFormat=D3DFMT_X8R8G8B8;params.BackBufferCount=1;params.SwapEffect=D3DSWAPEFFECT_DISCARD;params.hDeviceWindow=window;params.Windowed=TRUE;
    IDirect3DDevice8* device=nullptr;check(SUCCEEDED(api->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,&params,&device)),"Device");
    // Own shader with the fallback projection epilogue: mov r8,v0;
    // mov oPos,r8; mov oD0,v1. The bridge inserts its aspect correction.
    const DWORD program[]={0xfffe0101,1,0x800f0008,0x90e40000,1,0xc00f0000,0x80e40008,1,0xd00f0000,0x90e40001,0x0000ffff};
    const DWORD declaration[]={D3DVSD_STREAM(0),D3DVSD_REG(0,D3DVSDT_FLOAT4),D3DVSD_REG(1,D3DVSDT_D3DCOLOR),D3DVSD_END()};
    DWORD shader=0;check(SUCCEEDED(device->CreateVertexShader(declaration,program,&shader,0)),"Create fallback projection shader");
    struct Vertex{float x,y,z,w;DWORD color;};
    const Vertex quad[]={{-1,1,.5f,1,0xff22cc44},{1,1,.5f,1,0xff22cc44},{-1,-1,.5f,1,0xff22cc44},{1,-1,.5f,1,0xff22cc44}};
    for(unsigned pass=0;pass<3;++pass){
        if(pass)check(SUCCEEDED(device->Reset(&params)),"Reset with retained shader");
        check(SUCCEEDED(device->SetVertexShader(shader)),"Bind retained shader");
        device->SetRenderState(D3DRS_ZENABLE,FALSE);device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);device->SetRenderState(D3DRS_LIGHTING,FALSE);
        device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_DISABLE);device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
        check(SUCCEEDED(device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000,1,0)),"Clear output");
        check(SUCCEEDED(device->BeginScene()),"Begin shader scene");
        check(SUCCEEDED(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,quad,sizeof(Vertex))),"Draw retained shader");
        check(SUCCEEDED(device->EndScene()),"End shader scene");
        IDirect3DSurface8 *back=nullptr,*copy=nullptr;
        check(SUCCEEDED(device->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&back)),"Get output");
        check(SUCCEEDED(device->CreateImageSurface(1920,1080,D3DFMT_X8R8G8B8,&copy)),"Readback surface");
        check(SUCCEEDED(device->CopyRects(back,nullptr,0,copy,nullptr)),"Copy output");
        D3DLOCKED_RECT pixels={};check(SUCCEEDED(copy->LockRect(&pixels,nullptr,D3DLOCK_READONLY)),"Read output");
        const auto* row=reinterpret_cast<const DWORD*>(static_cast<const BYTE*>(pixels.pBits)+540*pixels.Pitch);
        std::printf("shader reset pass=%u center=%06lx\n",pass,row[960]&0xffffff);
        check((row[960]&0xffffff)==0x22cc44,"Retained projection shader must render after Reset");
        check((row[100]&0xffffff)==0,"Fallback aspect correction remains active across Reset");
        copy->UnlockRect();copy->Release();back->Release();
    }
    check(SUCCEEDED(device->DeleteVertexShader(shader)),"Delete retained shader");
    check(device->Release()==0,"Complete device release");api->Release();DestroyWindow(window);
    std::puts("PASS: retained projection shader renders across repeated Reset");
}
