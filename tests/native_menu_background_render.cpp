// Asset-free GPU consumer regression. Run only on an isolated Win32 desktop.
#define CINTERFACE
#define COBJMACROS
#include "../native/menu_background.h"
#include <cstdlib>
static bool backbuffer=true;
static bool target(){return backbuffer;}
static float hud_scale(){return .75f;}
static HRESULT WINAPI draw(IDirect3DDevice8* d,D3DPRIMITIVETYPE p,UINT n,const void* v,UINT s){return IDirect3DDevice8_DrawPrimitiveUP(d,p,n,v,s);}
static void check(bool value,const char* text){if(!value){std::fprintf(stderr,"FAIL: %s\n",text);std::exit(1);}}
static LRESULT CALLBACK procedure(HWND w,UINT m,WPARAM a,LPARAM b){return DefWindowProcA(w,m,a,b);}
static IDirect3DTexture8* texture(IDirect3DDevice8* d,unsigned width,const std::vector<DWORD>& pixels) {
    IDirect3DTexture8* t=nullptr;check(SUCCEEDED(IDirect3DDevice8_CreateTexture(d,width,64,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&t)),"Create owned texture");
    D3DLOCKED_RECT lock={};check(SUCCEEDED(IDirect3DTexture8_LockRect(t,0,&lock,nullptr,0)),"Lock owned texture");
    for(unsigned y=0;y<64;++y)std::memcpy(static_cast<BYTE*>(lock.pBits)+y*lock.Pitch,pixels.data()+y*width,width*4);
    check(SUCCEEDED(IDirect3DTexture8_UnlockRect(t,0)),"Unlock owned texture");return t;
}
static std::vector<DWORD> image(IDirect3DDevice8* d) {
    IDirect3DSurface8 *back=nullptr,*copy=nullptr;D3DSURFACE_DESC desc={};
    check(SUCCEEDED(IDirect3DDevice8_GetBackBuffer(d,0,D3DBACKBUFFER_TYPE_MONO,&back)),"Get output");
    check(SUCCEEDED(IDirect3DSurface8_GetDesc(back,&desc)),"Get pixel format");
    check(SUCCEEDED(IDirect3DDevice8_CreateImageSurface(d,1920,1080,desc.Format,&copy)),"Create readback");
    check(SUCCEEDED(IDirect3DDevice8_CopyRects(d,back,nullptr,0,copy,nullptr)),"Copy output");
    D3DLOCKED_RECT lock={};check(SUCCEEDED(IDirect3DSurface8_LockRect(copy,&lock,nullptr,D3DLOCK_READONLY)),"Read output");
    std::vector<DWORD> pixels(1920*1080);
    for(unsigned y=0;y<1080;++y)for(unsigned x=0;x<1920;++x){const BYTE* row=static_cast<const BYTE*>(lock.pBits)+y*lock.Pitch;
        if(desc.Format==D3DFMT_R5G6B5){const WORD p=reinterpret_cast<const WORD*>(row)[x];pixels[y*1920+x]=((p>>11)*255/31)<<16|(((p>>5)&63)*255/63)<<8|(p&31)*255/31;}
        else{check(desc.Format==D3DFMT_X8R8G8B8||desc.Format==D3DFMT_A8R8G8B8,"Supported output format");pixels[y*1920+x]=reinterpret_cast<const DWORD*>(row)[x]&0xffffff;}}
    IDirect3DSurface8_UnlockRect(copy);IDirect3DSurface8_Release(copy);IDirect3DSurface8_Release(back);return pixels;
}
static void save(const char* path,const std::vector<DWORD>& pixels){FILE* f=std::fopen(path,"wb");check(f!=nullptr,"Save private capture");std::fprintf(f,"P6\n1920 1080\n255\n");for(DWORD p:pixels){BYTE rgb[]={BYTE(p>>16),BYTE(p>>8),BYTE(p)};std::fwrite(rgb,3,1,f);}std::fclose(f);}
static DWORD shader(IDirect3DDevice8* device) {
    // Own minimal shader: add UI origin, scale X/Y, pass diffuse and UV.
    const DWORD program[]={0xfffe0101,
        2,0x80010000,0x90000000,0xa0aa0000,
        2,0x80020000,0x90550000,0xa0ff0000,
        5,0xc0010000,0x80000000,0xa0000000,
        5,0xc0020000,0x80550000,0xa0550000,
        1,0xc0040000,0xa0aa0001,1,0xc0080000,0xa0ff0001,
        1,0xd00f0000,0x90e40001,1,0xe0030000,0x90e40002,0x0000ffff};
    const DWORD declaration[]={D3DVSD_STREAM(0),D3DVSD_REG(0,D3DVSDT_FLOAT2),D3DVSD_REG(1,D3DVSDT_D3DCOLOR),D3DVSD_REG(2,D3DVSDT_FLOAT2),D3DVSD_END()};
    DWORD result=0;check(SUCCEEDED(IDirect3DDevice8_CreateVertexShader(device,declaration,program,&result,0)),"Create native-layout UI shader");
    return result;
}

int main() {
    WNDCLASSA cls={};cls.hInstance=GetModuleHandleA(nullptr);cls.lpfnWndProc=procedure;cls.lpszClassName="OSMMenuBackgroundRegression";RegisterClassA(&cls);
    HWND window=CreateWindowA(cls.lpszClassName,"OpenSoccerMania menu regression",WS_OVERLAPPEDWINDOW,0,0,640,480,nullptr,nullptr,cls.hInstance,nullptr);
    HMODULE module=LoadLibraryA(".\\d3d8.dll");check(module!=nullptr,"Load bridge");
    auto factory=reinterpret_cast<IDirect3D8* (WINAPI *)(UINT)>(GetProcAddress(module,"Direct3DCreate8"));check(factory!=nullptr,"Factory");
    IDirect3D8* api=factory(D3D_SDK_VERSION);check(api!=nullptr,"Create API");
    D3DPRESENT_PARAMETERS p={};p.BackBufferWidth=640;p.BackBufferHeight=480;p.BackBufferFormat=D3DFMT_X8R8G8B8;p.BackBufferCount=1;p.SwapEffect=D3DSWAPEFFECT_DISCARD;p.hDeviceWindow=window;p.Windowed=TRUE;
    IDirect3DDevice8* device=nullptr;check(SUCCEEDED(IDirect3D8_CreateDevice(api,0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,&p,&device)),"Create device");
    const DWORD ui_shader=shader(device);IDirect3DDevice8_SetVertexShader(device,ui_shader);
    const float c0[]={2.f/640,-2.f/480,-320.5f,-240.5f},c1[]={1,1,0,1};IDirect3DDevice8_SetVertexShaderConstant(device,0,c0,1);IDirect3DDevice8_SetVertexShaderConstant(device,1,c1,1);
    for(auto state:{std::pair<D3DRENDERSTATETYPE,DWORD>{D3DRS_ZENABLE,FALSE},{D3DRS_ZWRITEENABLE,FALSE},{D3DRS_FOGENABLE,FALSE},{D3DRS_CULLMODE,D3DCULL_NONE},{D3DRS_LIGHTING,FALSE},{D3DRS_ALPHABLENDENABLE,TRUE},{D3DRS_SRCBLEND,D3DBLEND_SRCALPHA},{D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA}})IDirect3DDevice8_SetRenderState(device,state.first,state.second);
    IDirect3DDevice8_SetTextureStageState(device,0,D3DTSS_COLOROP,D3DTOP_MODULATE);IDirect3DDevice8_SetTextureStageState(device,0,D3DTSS_COLORARG1,D3DTA_TEXTURE);IDirect3DDevice8_SetTextureStageState(device,0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);IDirect3DDevice8_SetTextureStageState(device,0,D3DTSS_ALPHAOP,D3DTOP_MODULATE);IDirect3DDevice8_SetTextureStageState(device,0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);IDirect3DDevice8_SetTextureStageState(device,0,D3DTSS_ALPHAARG2,D3DTA_DIFFUSE);IDirect3DDevice8_SetTextureStageState(device,1,D3DTSS_COLOROP,D3DTOP_DISABLE);
    for(auto state:{D3DTSS_MINFILTER,D3DTSS_MAGFILTER})IDirect3DDevice8_SetTextureStageState(device,0,state,D3DTEXF_POINT);
    IDirect3DTexture8* blue=texture(device,64,std::vector<DWORD>(4096,0xff204daa));
    struct Storage {IDirect3DTexture8* texture;};Storage storage{blue};
    struct Bitmap {BYTE prefix[0x30];Storage* storage;};Bitmap bitmap={};bitmap.storage=&storage;
    std::array<osm::MenuTile,80> tiles;
    check(SUCCEEDED(IDirect3DDevice8_Clear(device,0,nullptr,D3DCLEAR_TARGET,0xff284d98,1,0)),"Clear background");IDirect3DDevice8_BeginScene(device);
    IDirect3DDevice8_SetTexture(device,0,reinterpret_cast<IDirect3DBaseTexture8*>(blue));
    for(unsigned y=0;y<8;++y)for(unsigned x=0;x<10;++x){auto& t=tiles[y*10+x];const DWORD alpha=((x+y)&1)?0x50000000:0xc0000000;t={&bitmap,float(x*64),float(y*64),3,alpha|0xffffff};
        for(unsigned j=0;j<4;++j)t.vertices[j]={t.x+(j&1?64.f:0.f),t.y+(j&2?64.f:0.f),t.color-(j&2?0x10000000:0),j&1?1.f:0.f,j&2?1.f:0.f};
        check(SUCCEEDED(draw(device,D3DPT_TRIANGLESTRIP,2,t.vertices.data(),20)),"Draw original centered tile");}
    IDirect3DDevice8_EndScene(device);const auto before=image(device);save("menu-before.ppm",before);
    IDirect3DVertexBuffer8* stream=nullptr;IDirect3DDevice8_CreateVertexBuffer(device,80,0,0,D3DPOOL_MANAGED,&stream);IDirect3DDevice8_SetStreamSource(device,0,stream,20);
    IDirect3DDevice8_BeginScene(device);const auto replay=osm::menu_draw_side_tiles(device,{target,hud_scale,draw},tiles.data(),tiles.size(),{0,0,640,480});IDirect3DDevice8_EndScene(device);
    check(replay.planned==32&&replay.drawn==32,"Replay only the 32 required sidebar tiles");
    IDirect3DVertexBuffer8* restored=nullptr;UINT stride=0;IDirect3DDevice8_GetStreamSource(device,0,&restored,&stride);check(restored==stream&&stride==20,"Restore original stream binding");if(restored)IDirect3DVertexBuffer8_Release(restored);
    IDirect3DBaseTexture8* restored_texture=nullptr;IDirect3DDevice8_GetTexture(device,0,&restored_texture);check(restored_texture==reinterpret_cast<IDirect3DBaseTexture8*>(blue),"Restore original texture");IDirect3DBaseTexture8_Release(restored_texture);
    const auto after=image(device);save("menu-after.ppm",after);
    for(unsigned y=50;y<1000;y+=37)for(unsigned x=300;x<1600;x+=37)check(after[y*1920+x]==before[y*1920+x],"Central background and UI geometry remain unchanged");
    for(unsigned y=50;y<1000;y+=47)for(unsigned x=20;x<200;x+=29)check(after[y*1920+x]==after[y*1920+x+1440],"Sidebar repeats preserve native tile phase and gradient");
    check(after[300*1920+100]!=before[300*1920+100],"Pattern reaches the former flat sidebars");
    backbuffer=false;check(osm::menu_draw_side_tiles(device,{target,hud_scale,draw},tiles.data(),tiles.size(),{0,0,640,480}).drawn==0,"Do not alter render-texture paths");backbuffer=true;
    check(osm::controller_prompt_atlas().valid(),"Embedded four-family atlas is available");
    for(unsigned family=1;family<=4;++family)for(unsigned face=0;face<4;++face){
        const auto pixels=osm::controller_prompt_pixels(family,face);
        check(pixels.size()==4096,"Complete family face pixels");
        auto* prompt=texture(device,64,pixels);
        D3DLOCKED_RECT lock={};check(SUCCEEDED(IDirect3DTexture8_LockRect(prompt,0,&lock,nullptr,D3DLOCK_READONLY)),"Read uploaded family face");
        for(unsigned y=0;y<64;++y)check(!std::memcmp(static_cast<const BYTE*>(lock.pBits)+y*lock.Pitch,pixels.data()+y*64,256),"Family texture upload retains exact pixels");
        IDirect3DTexture8_UnlockRect(prompt,0);IDirect3DTexture8_Release(prompt);
    }
    IDirect3DTexture8* a=texture(device,64,osm::controller_prompt_pixels(1,0));IDirect3DTexture8* b=texture(device,64,osm::controller_prompt_pixels(1,1));
    IDirect3DTexture8* atlas=texture(device,128,std::vector<DWORD>(128*64,0xff555555));
    osm::menu_prompt_device=device;osm::menu_prompt_texture=b;IDirect3DDevice8_SetTexture(device,0,reinterpret_cast<IDirect3DBaseTexture8*>(blue));
    {osm::MenuPromptTextureSwap swap(device);IDirect3DDevice8_GetTexture(device,0,&restored_texture);check(restored_texture==reinterpret_cast<IDirect3DBaseTexture8*>(b),"Replace original 64x64 numeric art");IDirect3DBaseTexture8_Release(restored_texture);}
    IDirect3DDevice8_GetTexture(device,0,&restored_texture);check(restored_texture==reinterpret_cast<IDirect3DBaseTexture8*>(blue),"Restore source texture after prompt");IDirect3DBaseTexture8_Release(restored_texture);
    IDirect3DDevice8_SetTexture(device,0,reinterpret_cast<IDirect3DBaseTexture8*>(atlas));const auto swaps=osm::menu_prompt_swaps;
    {osm::MenuPromptTextureSwap swap(device);check(osm::menu_prompt_swaps==swaps,"Unknown atlas layout retains original art");}
    osm::menu_prompt_texture=nullptr;osm::menu_prompt_device=nullptr;
    IDirect3DDevice8_SetTexture(device,0,nullptr);IDirect3DDevice8_SetStreamSource(device,0,nullptr,0);IDirect3DTexture8_Release(atlas);IDirect3DTexture8_Release(a);IDirect3DTexture8_Release(b);IDirect3DTexture8_Release(blue);IDirect3DVertexBuffer8_Release(stream);IDirect3DDevice8_DeleteVertexShader(device,ui_shader);
    IDirect3DDevice8_Release(device);IDirect3D8_Release(api);DestroyWindow(window);std::puts("PASS: native-size sidebar pattern, original center, state restoration and scoped button texture replacement");
}

