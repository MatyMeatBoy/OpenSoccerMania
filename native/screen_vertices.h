#pragma once
#include <cstdint>
namespace osm {
// FVF bit zero is reserved. The game's programmable shader handles (5, 53,
// etc.) can otherwise look like XYZRHW when testing only the position mask.
inline bool screen_fvf(std::uint32_t value) {return !(value&1u)&&(value&0x0eu)==4u;}
inline float raster_coordinate(float value,float scale,float offset) {return offset+(value+.5f)*scale-.5f;}
inline bool overlay_depth(float z,float rhw) {return z>=0&&z<=1&&rhw>.9999f&&rhw<1.0001f;}
inline bool background_depth(float z,float rhw){return z>.99f&&z<1.01f&&rhw!=0&&rhw>-.001f&&rhw<.001f;}
}
