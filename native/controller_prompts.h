#pragma once
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace osm {
inline unsigned resolve_prompt_family(unsigned preference,unsigned detected) {
    return preference>=1&&preference<=4?preference:detected>=1&&detected<=4?detected:1;
}
inline unsigned prompt_family_from_flags(DWORD flags) {
    const unsigned value=(flags>>8)&7;
    return value>=1&&value<=4?value:1;
}
inline const char* prompt_style_label(unsigned style) {
    const char* labels[]={"Auto","Xbox 360","PlayStation 4","Switch Pro","Steam Deck"};
    return labels[style<=4?style:0];
}
inline const char* prompt_face_label(unsigned family,unsigned face) {
    static const char* labels[4][4]={{"A","B","X","Y"},{"Cross","Circle","Square","Triangle"},{"B","A","Y","X"},{"A","B","X","Y"}};
    return labels[resolve_prompt_family(0,family)-1][face<4?face:0];
}
class ControllerPromptAtlas {
    const unsigned char* payload_=nullptr;
    static DWORD word(const unsigned char* data,unsigned index){DWORD value;std::memcpy(&value,data+index*4,4);return value;}
public:
    ControllerPromptAtlas(const void* data,std::size_t bytes) {
        constexpr std::size_t payload_bytes=4*4*64*64*4;
        if(!data||bytes!=32+payload_bytes)return;
        const auto* header=static_cast<const unsigned char*>(data);
        if(std::memcmp(header,"OSMP",4)||word(header,1)!=1||word(header,2)!=64||word(header,3)!=64||word(header,4)!=4||word(header,5)!=4||word(header,6)!=payload_bytes)return;
        DWORD crc=0xffffffff;
        for(std::size_t i=32;i<bytes;++i){crc^=header[i];for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^(0xedb88320u&(0u-(crc&1)));}
        if((crc^0xffffffff)!=word(header,7))return;
        payload_=header+32;
    }
    bool valid()const{return payload_!=nullptr;}
    DWORD pixel(unsigned family,unsigned face,unsigned x,unsigned y)const {
        if(!payload_||family<1||family>4||face>=4||x>=64||y>=64)return 0;
        const unsigned index=(((family-1)*4+face)*4096+y*64+x)*4;
        DWORD value;std::memcpy(&value,payload_+index,4);return value;
    }
};
inline const ControllerPromptAtlas& controller_prompt_atlas() {
    static const ControllerPromptAtlas atlas=[] {
        HMODULE module=nullptr;
        if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCSTR>(&controller_prompt_atlas),&module))return ControllerPromptAtlas(nullptr,0);
        HRSRC resource=FindResourceA(module,MAKEINTRESOURCEA(102),RT_RCDATA);
        if(!resource)return ControllerPromptAtlas(nullptr,0);
        HGLOBAL loaded=LoadResource(module,resource);
        return ControllerPromptAtlas(loaded?LockResource(loaded):nullptr,SizeofResource(module,resource));
    }();
    return atlas;
}
inline std::vector<DWORD> controller_prompt_pixels(const ControllerPromptAtlas& atlas,unsigned family,unsigned face) {
    if(!atlas.valid()||family<1||family>4||face>=4)return {};
    std::vector<DWORD> pixels(4096);
    for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x) {
        DWORD color=atlas.pixel(family,face,x,y);
        // Software material highlight over the unmodified CC0 texture: a domed
        // Xbox face catches a broad upper reflection without changing silhouette.
        if(family==1&&(color>>24)>240&&std::hypot(float(x)-31.5f,float(y)-31.5f)<18.5f) {
            const float shine=.30f*std::exp(-std::pow((float(y)-20.f)/7.f,2.f))*std::max(0.f,1.f-std::pow((float(x)-27.f)/23.f,2.f));
            const auto channel=[&](unsigned shift){const float value=float((color>>shift)&255);return static_cast<DWORD>(std::lround(value+(255.f-value)*shine));};
            color=(color&0xff000000)|(channel(16)<<16)|(channel(8)<<8)|channel(0);
        }
        pixels[y*64+x]=color;
    }
    return pixels;
}
inline std::vector<DWORD> controller_prompt_pixels(unsigned family,unsigned face) {
    return controller_prompt_pixels(controller_prompt_atlas(),family,face);
}
inline bool paint_controller_prompt(DWORD* target,unsigned width,unsigned height,int left,int top,unsigned size,unsigned family,unsigned face) {
    const auto source=controller_prompt_pixels(family,face);
    if(!target||source.empty()||!size)return false;
    for(unsigned y=0;y<size;++y)for(unsigned x=0;x<size;++x) {
        const int tx=left+static_cast<int>(x),ty=top+static_cast<int>(y);
        if(tx<0||ty<0||static_cast<unsigned>(tx)>=width||static_cast<unsigned>(ty)>=height)continue;
        const DWORD src=source[(y*64/size)*64+x*64/size];const unsigned alpha=src>>24;if(!alpha)continue;
        auto& dst=target[ty*width+tx];DWORD result=0xff000000;
        for(unsigned shift:{0u,8u,16u})result|=(((((src>>shift)&255)*alpha+((dst>>shift)&255)*(255-alpha)+127)/255)<<shift);
        dst=result;
    }
    return true;
}
}
