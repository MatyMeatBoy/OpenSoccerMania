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
    for(double output:{1.,4./3.,16./9.,32./9.}) {
        // Disabled Hor+ preserves the source projection exactly at default zoom.
        const auto stock=osm::camera_projection(512,4./3.,output,false,1.);
        assert(stock.fov==512 && stock.aspect==4./3.);
        // Zoom must still work in 4:3 and with Hor+ disabled at any output size.
        for(double zoom:{.85,.70}) {
            const auto zoomed=osm::camera_projection(512,4./3.,output,false,zoom);
            assert(zoomed.aspect==4./3. && zoomed.fov>512);
            const double before=(4./3.)/std::tan(512*half_lens_unit);
            const double after=zoomed.aspect/std::tan(zoomed.fov*half_lens_unit);
            assert(std::fabs(after/before-zoom)<.003);
            const auto wide=osm::camera_projection(512,4./3.,output,true,zoom);
            assert(wide.aspect==output);
            const double wide_vertical=wide.aspect/std::tan(wide.fov*half_lens_unit);
            assert(std::fabs(wide_vertical/before-zoom)<.003);
        }
    }
    const auto bad_output=osm::camera_projection(512,4./3.,NAN,false,.7);
    assert(bad_output.aspect==4./3. && bad_output.fov>512);
    const auto unchanged=osm::camera_projection(512,4./3.,NAN,true,.7);
    assert(unchanged.fov==512 && unchanged.aspect==4./3.);
    const auto invalid_zoom=osm::camera_projection(512,4./3.,16./9.,false,NAN);
    assert(invalid_zoom.fov==512 && invalid_zoom.aspect==4./3.);
}
