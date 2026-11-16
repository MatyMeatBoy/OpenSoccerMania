#include "../native/screen_vertices.h"
#include <cassert>
#include <cmath>
int main() {
    assert(osm::screen_fvf(0x1c4));
    assert(osm::screen_fvf(0x104));
    assert(!osm::screen_fvf(5));
    assert(!osm::screen_fvf(53));
    assert(!osm::screen_fvf(3));
    assert(!osm::screen_fvf(0x112));
    assert(osm::overlay_depth(0.f,1.f));
    assert(!osm::overlay_depth(1.0015f,0.00002f)); // captured sky vertices
    assert(!osm::overlay_depth(.9f,.01f)); // projected scene geometry
    assert(osm::background_depth(1.0015f,.00002f));
    assert(osm::background_depth(1.0017704f,-.000011315f));
    assert(osm::background_depth(.9998924f,.000115797f));
    assert(!osm::background_depth(1.0015f,0));
    assert(!osm::background_depth(1.0015f,NAN));
    assert(!osm::background_depth(.9f,.00002f));
    assert(!osm::background_depth(1.0015f,1.f));
    assert(std::fabs(osm::raster_coordinate(-.5f,3.f,0)+.5f)<.0001f);
    assert(std::fabs(osm::raster_coordinate(639.5f,3.f,0)-1919.5f)<.0001f);
}
