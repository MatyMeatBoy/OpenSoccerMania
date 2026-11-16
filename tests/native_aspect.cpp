#include "../native/aspect.h"
#include <cassert>
#include <cmath>
int main() {
    assert(osm::horizontal_scale(4.0f/3.0f) == 1.0f);
    assert(std::fabs(osm::horizontal_scale(16.0f/9.0f)-0.75f)<0.00001f);
    assert(std::fabs(osm::horizontal_scale(32.0f/9.0f)-0.375f)<0.00001f);
    assert(std::fabs(osm::hud_x(319.5f,0,640,0.75f)-319.5f)<0.00001f);
    assert(std::fabs(osm::hud_x(0,0,640,0.75f)-79.875f)<0.00001f);
    assert(osm::perspective(1,0));
    assert(!osm::perspective(0,1));
}
