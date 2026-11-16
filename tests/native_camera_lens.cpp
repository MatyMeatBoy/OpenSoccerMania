#include "../native/camera_lens.h"
#include <cassert>
#include <cmath>
#include <initializer_list>
int main() {
    // Independent reference from the original projection-builder constants.
    constexpr double half_lens_unit=(1./11.)*3.14159265358979323846/360.;
    for(int fov:{400,512,700,900}) {
        const double original=(4./3.)/std::tan(fov*half_lens_unit);
        for(double aspect:{1.,4./3.,16./9.,21./9.,32./9.}) {
            const int wide=osm::horplus_fov(fov,4./3.,aspect);
            assert(wide>0&&wide<1980);
            const double vertical=aspect/std::tan(wide*half_lens_unit);
            assert(std::fabs(vertical/original-1)<.003);
            const int distant=osm::horplus_fov(fov,4./3.,aspect,.7);
            assert(distant>wide);
            const double distant_vertical=aspect/std::tan(distant*half_lens_unit);
            assert(std::fabs(distant_vertical/original-.7)<.003);
        }
        assert(osm::horplus_fov(fov,4./3.,4./3.)==fov);
        assert(osm::horplus_fov(fov,4./3.,1)<fov);
    }
    assert(osm::horplus_fov(512,4./3.,16./9.)==656);
    assert(osm::horplus_fov(512,NAN,16./9.)==512);
}
