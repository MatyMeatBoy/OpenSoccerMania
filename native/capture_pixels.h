#pragma once
#include <cstdint>
namespace osm {
inline std::uint32_t capture_rgb565(std::uint16_t pixel) {
    const unsigned red=((pixel>>11)&31)*255/31;
    const unsigned green=((pixel>>5)&63)*255/63;
    const unsigned blue=(pixel&31)*255/31;
    return 0xff000000u|(red<<16)|(green<<8)|blue;
}
}
