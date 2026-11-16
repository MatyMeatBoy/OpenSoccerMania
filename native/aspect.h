#pragma once
#include <cmath>

namespace osm {
inline float horizontal_scale(float output_aspect, float original_aspect = 4.0f / 3.0f) {
    return output_aspect > original_aspect ? original_aspect / output_aspect : 1.0f;
}
inline float hud_x(float x, float viewport_x, float viewport_width, float scale) {
    const float center = viewport_x + viewport_width * 0.5f - 0.5f;
    return center + (x - center) * scale;
}
inline bool perspective(float m34, float m44) {
    return std::fabs(m34) > 0.5f && std::fabs(m44) < 0.001f;
}
}
