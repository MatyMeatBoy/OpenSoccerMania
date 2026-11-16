#pragma once
#include <cmath>
namespace osm {
// Projection builder 0x41b510 converts lens units with (1/11)*(pi/360).
// These are 11 units per degree, distinct from the engine's 4096-unit rotations.
inline int horplus_fov(int original,double original_aspect,double output_aspect,double zoom=1.) {
    if(original<=0||original>=1980||!std::isfinite(original_aspect)||!std::isfinite(output_aspect)||
       !std::isfinite(zoom)||original_aspect<=0||output_aspect<=0||zoom<.25||zoom>4)return original;
    constexpr double radians_per_half_lens_unit=3.14159265358979323846/3960.;
    const double half=original*radians_per_half_lens_unit;
    return static_cast<int>(std::lround(std::atan(std::tan(half)*output_aspect/original_aspect/zoom)/radians_per_half_lens_unit));
}
}
