#pragma once
#include <windows.h>
#include <d3d8.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>
#include "../third_party/minhook/include/MinHook.h"

namespace osm {
inline thread_local bool title_background_drawing=false;
struct TitleBackgroundScope {bool previous;TitleBackgroundScope():previous(title_background_drawing){title_background_drawing=true;}~TitleBackgroundScope(){title_background_drawing=previous;}};
struct TitleBmp {unsigned width=0,height=0;std::vector<unsigned char> bgra;};
inline unsigned title_u32(const unsigned char* p){return unsigned(p[0])|(unsigned(p[1])<<8)|(unsigned(p[2])<<16)|(unsigned(p[3])<<24);}
inline bool decode_title_bmp(const std::vector<unsigned char>& bytes,TitleBmp& out) {
    out={};
    // Accept only the compositor's bounded, explicit-alpha BITMAPV4 format.
    constexpr size_t size=122+2048*512*4;
    if(bytes.size()!=size||bytes[0]!='B'||bytes[1]!='M'||title_u32(bytes.data()+2)!=size||
       title_u32(bytes.data()+10)!=122||title_u32(bytes.data()+14)!=108||
       title_u32(bytes.data()+18)!=2048||title_u32(bytes.data()+22)!=512||
       bytes[26]!=1||bytes[27]!=0||bytes[28]!=32||bytes[29]!=0||title_u32(bytes.data()+30)!=3||
       title_u32(bytes.data()+34)!=2048*512*4||title_u32(bytes.data()+54)!=0xff0000||
       title_u32(bytes.data()+58)!=0xff00||title_u32(bytes.data()+62)!=0xff||title_u32(bytes.data()+66)!=0xff000000)return false;
    out.width=2048;out.height=512;out.bgra.resize(2048*512*4);
    for(unsigned y=0;y<512;++y) {
        const auto* row=bytes.data()+122+(511-y)*2048*4;
        for(unsigned x=0;x<2048;++x) {
            const bool center=x>=682&&x<1365;
            if(row[x*4+3]!=(center?0:255)) {out={};return false;}
            if(center&&(row[x*4]||row[x*4+1]||row[x*4+2])) {out={};return false;}
        }
        std::memcpy(out.bgra.data()+y*2048*4,row,2048*4);
    }
    return true;
}
struct TitleSide {float x0,x1,u0,u1;};
inline bool title_sides(unsigned width,unsigned height,TitleSide (&sides)[2]) {
    if(!height||!width||width>16384||height>16384)return false;
    const float center=height*4.f/3.f,left=(width-center)*.5f,right=left+center;
    if(left<=0)return false;
    const float span=std::min(1.f,left/(height*(4.f-4.f/3.f)*.5f));
    constexpr float l=682.f/2048.f,r=1365.f/2048.f;
    sides[0]={0,left,l*(1-span),l};sides[1]={right,float(width),r,r+(1-r)*span};return true;
}
class TitleBackground {
    IDirect3DDevice8* device_=nullptr;IDirect3DTexture8* texture_=nullptr;
    std::string path_;
public:
    unsigned last_width=0,last_height=0;
    ~TitleBackground(){release();}
    bool owns_device(IDirect3DDevice8* device)const{return device_==device;}
    void release(){auto* texture=texture_;texture_=nullptr;device_=nullptr;if(texture)IDirect3DTexture8_Release(texture);}
    void set_device(IDirect3DDevice8* device){if(device_!=device){release();device_=device;}}
    void configure(const char* config){char path[32768]={};GetPrivateProfileStringA("UI","TitleBackgroundPath","",path,sizeof(path),config);path_=path;
        if(!path_.empty()&&path_[0]!='\\'&&path_[0]!='/'&&!(path_.size()>1&&path_[1]==':')){
            const std::string profile=config?config:"";const auto slash=profile.find_last_of("\\/");
            if(slash!=std::string::npos)path_=profile.substr(0,slash+1)+path_;
        }
    }
    bool load(){
        if(texture_)return true;if(!device_||path_.empty())return false;
        FILE* file=std::fopen(path_.c_str(),"rb");if(!file)return false;
        std::vector<unsigned char> bytes(122+2048*512*4+1);const auto n=std::fread(bytes.data(),1,bytes.size(),file);const bool bad=std::ferror(file);std::fclose(file);bytes.resize(n);
        TitleBmp bmp;if(bad||!decode_title_bmp(bytes,bmp))return false;
        IDirect3DTexture8* texture=nullptr;
        if(FAILED(IDirect3DDevice8_CreateTexture(device_,bmp.width,bmp.height,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&texture)))return false;
        D3DLOCKED_RECT lock={};if(FAILED(IDirect3DTexture8_LockRect(texture,0,&lock,nullptr,0))){IDirect3DTexture8_Release(texture);return false;}
        for(unsigned y=0;y<bmp.height;++y)std::memcpy(static_cast<unsigned char*>(lock.pBits)+y*lock.Pitch,bmp.bgra.data()+y*bmp.width*4,bmp.width*4);
        IDirect3DTexture8_UnlockRect(texture,0);texture_=texture;return true;
    }
    bool draw(){
        TitleBackgroundScope physical_scope;
        if(!load())return false;
        IDirect3DSurface8* target=nullptr;D3DSURFACE_DESC desc={};
        if(FAILED(IDirect3DDevice8_GetRenderTarget(device_,&target)))return false;
        const auto desc_result=IDirect3DSurface8_GetDesc(target,&desc);IDirect3DSurface8_Release(target);if(FAILED(desc_result))return false;last_width=desc.Width;last_height=desc.Height;
        TitleSide sides[2];if(!title_sides(desc.Width,desc.Height,sides))return false;
        DWORD state=0;D3DVIEWPORT8 old_view={};
        if(FAILED(IDirect3DDevice8_GetViewport(device_,&old_view))||FAILED(IDirect3DDevice8_CreateStateBlock(device_,D3DSBT_ALL,&state)))return false;
        IDirect3DVertexBuffer8* old_stream=nullptr;UINT old_stride=0;
        if(FAILED(IDirect3DDevice8_GetStreamSource(device_,0,&old_stream,&old_stride))){IDirect3DDevice8_DeleteStateBlock(device_,state);return false;}
        D3DVIEWPORT8 viewport={0,0,desc.Width,desc.Height,0,1};IDirect3DDevice8_SetViewport(device_,&viewport);
        IDirect3DDevice8_SetVertexShader(device_,D3DFVF_XYZRHW|D3DFVF_TEX1);IDirect3DDevice8_SetPixelShader(device_,0);
        IDirect3DDevice8_SetTexture(device_,0,static_cast<IDirect3DBaseTexture8*>(reinterpret_cast<void*>(texture_)));
        IDirect3DDevice8_SetRenderState(device_,D3DRS_ZENABLE,FALSE);IDirect3DDevice8_SetRenderState(device_,D3DRS_ZWRITEENABLE,FALSE);
        IDirect3DDevice8_SetRenderState(device_,D3DRS_ALPHABLENDENABLE,FALSE);IDirect3DDevice8_SetRenderState(device_,D3DRS_ALPHATESTENABLE,FALSE);
        IDirect3DDevice8_SetRenderState(device_,D3DRS_CULLMODE,D3DCULL_NONE);IDirect3DDevice8_SetRenderState(device_,D3DRS_FOGENABLE,FALSE);
        IDirect3DDevice8_SetRenderState(device_,D3DRS_STENCILENABLE,FALSE);IDirect3DDevice8_SetRenderState(device_,D3DRS_FILLMODE,D3DFILL_SOLID);
        IDirect3DDevice8_SetRenderState(device_,D3DRS_COLORWRITEENABLE,15);
        IDirect3DDevice8_SetRenderState(device_,D3DRS_CLIPPLANEENABLE,0);
        IDirect3DDevice8_SetRenderState(device_,D3DRS_WRAP0,0);
        IDirect3DDevice8_SetTextureStageState(device_,0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);IDirect3DDevice8_SetTextureStageState(device_,0,D3DTSS_COLORARG1,D3DTA_TEXTURE);
        IDirect3DDevice8_SetTextureStageState(device_,0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);IDirect3DDevice8_SetTextureStageState(device_,0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);
        IDirect3DDevice8_SetTextureStageState(device_,0,D3DTSS_TEXCOORDINDEX,0);IDirect3DDevice8_SetTextureStageState(device_,0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE);
        IDirect3DDevice8_SetTextureStageState(device_,0,D3DTSS_ADDRESSU,D3DTADDRESS_CLAMP);IDirect3DDevice8_SetTextureStageState(device_,0,D3DTSS_ADDRESSV,D3DTADDRESS_CLAMP);
        IDirect3DDevice8_SetTextureStageState(device_,0,D3DTSS_MINFILTER,D3DTEXF_LINEAR);IDirect3DDevice8_SetTextureStageState(device_,0,D3DTSS_MAGFILTER,D3DTEXF_LINEAR);
        IDirect3DDevice8_SetTextureStageState(device_,0,D3DTSS_MIPFILTER,D3DTEXF_NONE);IDirect3DDevice8_SetTextureStageState(device_,1,D3DTSS_COLOROP,D3DTOP_DISABLE);
        struct Vertex{float x,y,z,rhw,u,v;};HRESULT result=S_OK;
        for(const auto& s:sides){
            // Texel-center UVs prevent sampling the transparent central region.
            const float u0=std::clamp(s.u0,0.5f/2048.f,681.5f/2048.f),u1=std::clamp(s.u1,0.5f/2048.f,681.5f/2048.f);
            const float a=s.x0==0?u0:std::clamp(s.u0,1365.5f/2048.f,2047.5f/2048.f);
            const float b=s.x0==0?u1:std::clamp(s.u1,1365.5f/2048.f,2047.5f/2048.f);
            Vertex vertices[]={{s.x0-.5f,-.5f,0,1,a,.5f/512},{s.x1-.5f,-.5f,0,1,b,.5f/512},{s.x0-.5f,desc.Height-.5f,0,1,a,511.5f/512},{s.x1-.5f,desc.Height-.5f,0,1,b,511.5f/512}};
            const auto hr=IDirect3DDevice8_DrawPrimitiveUP(device_,D3DPT_TRIANGLESTRIP,2,vertices,sizeof(Vertex));if(FAILED(hr))result=hr;
        }
        const auto restore=IDirect3DDevice8_ApplyStateBlock(device_,state);IDirect3DDevice8_DeleteStateBlock(device_,state);
        IDirect3DDevice8_SetViewport(device_,&old_view);IDirect3DDevice8_SetStreamSource(device_,0,old_stream,old_stride);if(old_stream)IDirect3DVertexBuffer8_Release(old_stream);
        return SUCCEEDED(result)&&SUCCEEDED(restore);
    }
};
static TitleBackground title_background;
using TitleImageDraw=int (__cdecl *)(void*,int,int);using TitleTransition=int (__cdecl *)();
static TitleImageDraw original_title_image=nullptr;static TitleTransition original_title_transition=nullptr;
static thread_local bool title_frame=false;
static std::string title_hook_log;
static bool title_draw_reported=false;
inline void reset_title_frame(){title_frame=false;}
inline bool title_draw_context(std::uintptr_t caller){return caller==0x4f2f86||caller==0x4f30a1;}
static int __cdecl title_image_draw(void* image,int a,int b){if(title_draw_context(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0))))title_frame=true;return original_title_image(image,a,b);}
static int __cdecl title_transition_draw(){const bool draw=title_frame;title_frame=false;if(draw){reinterpret_cast<void (__cdecl *)()>(0x415320)();const bool result=title_background.draw();
    if(!title_draw_reported){title_draw_reported=true;FILE* file=title_hook_log.empty()?nullptr:std::fopen(title_hook_log.c_str(),"a");if(file){std::fprintf(file,"TITLE_BACKGROUND first-draw result=%s physical=%ux%u center-preserved=1\n",result?"ok":"failed",title_background.last_width,title_background.last_height);std::fclose(file);}}}return original_title_transition();}
inline bool title_hook_signature(const unsigned char* base,size_t size){
    if(!base||size<0xf30a5)return false;
    const unsigned char image[]={0x8b,0x54,0x24,0x04,0x83,0xec,0x14,0x8b,0x82,0x80,0,0,0,0x83,0xf8,0x08};
    const unsigned char flush[]={0x6a,1,0xe8,0x99,0x0d,0,0,0x59,0xc3};
    const unsigned char transition[]={0x56,0x57,0x33,0xf6,0xe8,0x77,0x77,0xf6,0xff,0x8b,0xf8,0xa1,0x0c,0x13,0x66,0};
    const unsigned char call1[]={0xe8,0xca,0x28,0xf2,0xff},call2[]={0xe8,0xaf,0x27,0xf2,0xff};
    return !std::memcmp(base+0x15850,image,sizeof(image))&&!std::memcmp(base+0x15320,flush,sizeof(flush))&&!std::memcmp(base+0xe2940,transition,sizeof(transition))&&!std::memcmp(base+0xf2f81,call1,5)&&!std::memcmp(base+0xf309c,call2,5);
}
inline void start_title_hooks(const char* config,const char* log_path){
    title_background.configure(config);title_hook_log=log_path?log_path:"";
    if(!GetPrivateProfileIntA("Graphics","TitleExtension",1,config))return;
    if(!GetPrivateProfileIntA("Graphics","VerifiedExecutable",0,config))return;
    const auto* base=reinterpret_cast<const unsigned char*>(GetModuleHandleA(nullptr));if(reinterpret_cast<std::uintptr_t>(base)!=0x400000)return;
    const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<0||dos->e_lfanew>1024)return;
    const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS*>(base+dos->e_lfanew);if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC)return;
    for(size_t offset:{size_t(0x15850),size_t(0x15320),size_t(0xe2940),size_t(0xf2f81),size_t(0xf309c)}) {
        MEMORY_BASIC_INFORMATION memory={};
        if(!VirtualQuery(base+offset,&memory,sizeof(memory))||memory.State!=MEM_COMMIT||memory.Type!=MEM_IMAGE||
           (memory.Protect&(PAGE_NOACCESS|PAGE_GUARD))||reinterpret_cast<std::uintptr_t>(base+offset+16)>
           reinterpret_cast<std::uintptr_t>(memory.BaseAddress)+memory.RegionSize)return;
    }
    if(!title_hook_signature(base,nt->OptionalHeader.SizeOfImage))return;
    const auto a=MH_CreateHook(reinterpret_cast<void*>(0x415850),reinterpret_cast<void*>(title_image_draw),reinterpret_cast<void**>(&original_title_image));
    const auto b=MH_CreateHook(reinterpret_cast<void*>(0x4e2940),reinterpret_cast<void*>(title_transition_draw),reinterpret_cast<void**>(&original_title_transition));
    if(a!=MH_OK||b!=MH_OK){if(a==MH_OK)MH_RemoveHook(reinterpret_cast<void*>(0x415850));if(b==MH_OK)MH_RemoveHook(reinterpret_cast<void*>(0x4e2940));}
    FILE* log=log_path?std::fopen(log_path,"a"):nullptr;if(log){std::fprintf(log,"TITLE_BACKGROUND hooks=%d,%d center-preserved transition-coverage-unverified\n",int(a),int(b));std::fclose(log);}
}
} // namespace osm
