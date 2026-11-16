#pragma once
#include <cmath>
namespace osm {
// The original camera stores horizontal FOV in 4096 angular units per turn.
inline int horplus_fov(int original,double original_aspect,double output_aspect) {
    if(original<=0||original>=2048||original_aspect<=0||output_aspect<=original_aspect)return original;
    constexpr double tau=6.2831853071795864769;
    const double half=original*tau/8192.;
    return static_cast<int>(std::lround(std::atan(std::tan(half)*output_aspect/original_aspect)*8192./tau));
}
}
