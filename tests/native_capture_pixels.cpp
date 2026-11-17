#include "../native/capture_pixels.h"
#include <cassert>
int main(){
    assert(osm::capture_rgb565(0)==0xff000000u);
    assert(osm::capture_rgb565(0xffff)==0xffffffffu);
    assert(osm::capture_rgb565(0xf800)==0xffff0000u);
    assert(osm::capture_rgb565(0x07e0)==0xff00ff00u);
    assert(osm::capture_rgb565(0x001f)==0xff0000ffu);
    assert(osm::capture_rgb565(0x8410)==0xff838183u);
}
