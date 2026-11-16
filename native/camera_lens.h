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
struct CameraProjection {int fov;double aspect;};
// Zoom is independent of aspect correction. With Hor+ disabled, use the source
// aspect for both sides of the lens calculation, preserving original framing.
inline CameraProjection camera_projection(int fov,double source_aspect,double output_aspect,bool horplus,double zoom) {
    const CameraProjection source{fov,source_aspect};
    if(fov<=0||fov>=1980||!std::isfinite(source_aspect)||source_aspect<=0||
       !std::isfinite(zoom)||zoom<.25||zoom>4)return source;
    if(horplus&&(!std::isfinite(output_aspect)||output_aspect<=0))return source;
    const double aspect=horplus?output_aspect:source_aspect;
    return {horplus_fov(fov,source_aspect,aspect,zoom),aspect};
}
}
