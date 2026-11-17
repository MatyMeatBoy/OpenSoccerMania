#pragma once
#include <windows.h>
#include <d3d8.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include "screen_vertices.h"
#include "../third_party/minhook/include/MinHook.h"
namespace osm {
inline bool preview_viewport_needed(DWORD shader,bool programmable,bool clip_shader) {
    return !clip_shader&&(programmable||!screen_fvf(shader));
}
inline double preview_axis(double value,double center,double scale) {
    return std::isfinite(scale)&&scale>0&&scale<=1?center+(value-center)*scale:value;
}
inline D3DVIEWPORT8 preview_viewport(const D3DVIEWPORT8& source,double center_x,double center_y,double x_scale,double y_scale) {
    const double x1=std::floor(preview_axis(source.X,center_x,x_scale)),y1=std::floor(preview_axis(source.Y,center_y,y_scale));
    const double x2=std::ceil(preview_axis(double(source.X)+source.Width,center_x,x_scale)),y2=std::ceil(preview_axis(double(source.Y)+source.Height,center_y,y_scale));
    auto result=source;result.X=static_cast<DWORD>(std::max(0.,x1));result.Y=static_cast<DWORD>(std::max(0.,y1));
    result.Width=static_cast<DWORD>(std::max(1.,x2-result.X));result.Height=static_cast<DWORD>(std::max(1.,y2-result.Y));return result;
}
class PreviewScope {
    unsigned* depth_;
public:
    PreviewScope(unsigned& depth,bool active):depth_(active?&depth:nullptr){if(depth_)++*depth_;}
    ~PreviewScope(){if(depth_)--*depth_;}
    PreviewScope(const PreviewScope&)=delete;
    PreviewScope& operator=(const PreviewScope&)=delete;
};
static thread_local unsigned preview_model_depth=0,preview_clear_depth=0;
static bool preview_hooks_active=false;
inline bool preview_camera_context(std::uintptr_t caller,bool active,bool model_active) {
    return active&&(caller==0x509cce||(model_active&&caller==0x4318bf));
}
using PreviewModel=int (__cdecl *)(void*,void*);
using PreviewClear=int (__cdecl *)(const void*,DWORD,DWORD);
static PreviewModel original_preview_model;
static PreviewClear original_preview_clear;
static int __cdecl preview_model(void* model,void* camera) {
    PreviewScope scope(preview_model_depth,reinterpret_cast<std::uintptr_t>(__builtin_return_address(0))==0x509dda);
    return original_preview_model(model,camera);
}
static int __cdecl preview_clear(const void* rect,DWORD color,DWORD flags) {
    PreviewScope scope(preview_clear_depth,reinterpret_cast<std::uintptr_t>(__builtin_return_address(0))==0x509dbc);
    return original_preview_clear(rect,color,flags);
}
inline void start_preview_hooks(const char* config,const char* log,bool camera_ready) {
    if(!GetPrivateProfileIntA("Graphics","VerifiedExecutable",0,config))return;
    if(!camera_ready)return;
    auto* base=reinterpret_cast<unsigned char*>(GetModuleHandleA(nullptr));
    if(reinterpret_cast<std::uintptr_t>(base)!=0x400000)return;
    const BYTE model_entry[]={0xa1,0xb0,0xea,0x61,0x00,0x81,0xec,0x0c,0x04,0x00,0x00,0xa3,0xc8,0xea,0x61,0x00};
    const BYTE clear_entry[]={0x83,0xec,0x10,0x53,0x55,0x56,0x57};
    const BYTE model_call[]={0x53,0x52,0xe8,0xa6,0x7a,0xf2,0xff};
    const BYTE clear_call[]={0xe8,0x34,0xb4,0xf0,0xff};
    const BYTE camera_call[]={0x53,0x51,0xe8,0xc2,0x11,0xf1,0xff};
    if(std::memcmp(base+0x31880,model_entry,sizeof(model_entry))||std::memcmp(base+0x151f0,clear_entry,sizeof(clear_entry))||
       std::memcmp(base+0x109dd3,model_call,sizeof(model_call))||std::memcmp(base+0x109db7,clear_call,sizeof(clear_call))||
       std::memcmp(base+0x109cc7,camera_call,sizeof(camera_call))) {
        FILE* file=std::fopen(log,"a");if(file){std::fputs("MENU_PREVIEW unsupported code; hooks skipped\n",file);std::fclose(file);}return;
    }
    const auto a=MH_CreateHook(base+0x31880,reinterpret_cast<void*>(preview_model),reinterpret_cast<void**>(&original_preview_model));
    const auto b=MH_CreateHook(base+0x151f0,reinterpret_cast<void*>(preview_clear),reinterpret_cast<void**>(&original_preview_clear));
    if(a!=MH_OK||b!=MH_OK){if(a==MH_OK)MH_RemoveHook(base+0x31880);if(b==MH_OK)MH_RemoveHook(base+0x151f0);}
    preview_hooks_active=a==MH_OK&&b==MH_OK;
    FILE* file=std::fopen(log,"a");if(file){std::fprintf(file,"MENU_PREVIEW hooks=%d,%d active=%d\n",int(a),int(b),int(a==MH_OK&&b==MH_OK));std::fclose(file);}
}
}
