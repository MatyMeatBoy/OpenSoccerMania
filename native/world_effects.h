#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cmath>
namespace osm {
inline bool world_stencil_signature(const unsigned char* image,std::size_t size,bool verified) {
    constexpr unsigned char entry[]={0xa1,0x00,0xb8,0x65,0x00,0x85,0xc0,0x0f,0x84,0x1d,0x01,0x00,0x00};
    constexpr unsigned char draw[]={0x6a,0x44,0x50,0xff,0x91,0x30,0x01,0x00,0x00,0xa1,0x28,0x6c,0x61,0x00,0x6a,0x14,0x68,0x20,0xba,0x65,0x00,0x6a,0x02,0x8b,0x10,0x6a,0x05,0x50,0xff,0x92,0x20,0x01,0x00,0x00};
    return verified&&image&&size>=0x25ba70&&
        !std::memcmp(image+0xc9100,entry,sizeof(entry))&&
        !std::memcmp(image+0xc91ee,draw,sizeof(draw));
}
struct WorldStencilVertex {float x,y,z,rhw;std::uint32_t color;};
static_assert(sizeof(WorldStencilVertex)==20,"Known stencil composite vertex layout");
inline bool world_stencil_composite(bool supported,std::uintptr_t source,std::uint32_t format,
                                    unsigned primitive,unsigned count,unsigned stride,bool stencil,
                                    const void* data,unsigned width,unsigned height) {
    // This is SOShadowStencil's black full-screen resolve, not an ordinary HUD
    // quad. Depth/RHW alone cannot distinguish those two draw paths.
    if(!supported||source!=0x65ba20||format!=0x44||primitive!=5||count!=2||
       stride!=sizeof(WorldStencilVertex)||!stencil||!data||width<2||height<2)return false;
    WorldStencilVertex v[4];std::memcpy(v,data,sizeof(v));
    if(!(v[0].color>>24)||(v[0].color&0xffffff))return false;
    for(unsigned i=0;i<4;++i) {
        const float x=(i&1)?static_cast<float>(width-1):0.f;
        const float y=(i&2)?static_cast<float>(height-1):0.f;
        if(!std::isfinite(v[i].x)||!std::isfinite(v[i].y)||std::fabs(v[i].x-x)>.01f||
           std::fabs(v[i].y-y)>.01f||v[i].z!=0.f||v[i].rhw!=1.f||v[i].color!=v[0].color)return false;
    }
    return true;
}
inline void world_stencil_edges(WorldStencilVertex* vertices,unsigned width,unsigned height) {
    // The legacy producer uses inclusive pixel indices. A physical full-screen
    // resolve needs pixel-edge coordinates before the half-pixel raster map.
    for(unsigned i=0;i<4;++i){vertices[i].x=(i&1)?width-.5f:-.5f;vertices[i].y=(i&2)?height-.5f:-.5f;}
}
}
