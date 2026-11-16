#include "../native/camera_lens.h"
#include <cassert>
#include <cmath>
#include <initializer_list>
int main() {
    constexpr double tau=6.2831853071795864769;
    for(int fov:{400,512,700,900}) {
        const double original=(4./3.)/std::tan(fov*tau/8192.);
        for(double aspect:{16./9.,21./9.,32./9.}) {
            const int wide=osm::horplus_fov(fov,4./3.,aspect);
            assert(wide>fov&&wide<2048);
            const double vertical=aspect/std::tan(wide*tau/8192.);
            assert(std::fabs(vertical/original-1)<.003);
        }
        assert(osm::horplus_fov(fov,4./3.,4./3.)==fov);
        assert(osm::horplus_fov(fov,4./3.,1)==fov);
    }
}
