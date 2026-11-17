#include "../native/menu_preview.h"
#include <cassert>
#include <limits>
int main() {
    assert(osm::preview_viewport_needed(31,true,false));
    assert(osm::preview_viewport_needed(D3DFVF_XYZ|D3DFVF_DIFFUSE,false,false));
    assert(!osm::preview_viewport_needed(D3DFVF_XYZRHW|D3DFVF_DIFFUSE,false,false));
    assert(!osm::preview_viewport_needed(5,true,true));
    assert(osm::preview_camera_context(0x509cce,true,false));
    assert(osm::preview_camera_context(0x4318bf,true,true));
    assert(!osm::preview_camera_context(0x4318bf,true,false));
    assert(!osm::preview_camera_context(0x509cce,false,true));
    assert(!osm::preview_camera_context(0x406d4a,true,true));
    D3DVIEWPORT8 full={0,0,1920,1080,.1f,.9f};
    auto centered=osm::preview_viewport(full,960,540,.75,1);
    assert(centered.X==240&&centered.Y==0&&centered.Width==1440&&centered.Height==1080);
    assert(centered.MinZ==full.MinZ&&centered.MaxZ==full.MaxZ);
    D3DVIEWPORT8 partial={300,225,300,225,0,1};
    auto local=osm::preview_viewport(partial,960,540,.75,1);
    assert(local.X==465&&local.Y==225&&local.Width==225&&local.Height==225);
    // The logical clear and physical model viewport must cover the same cells.
    assert(osm::preview_axis(100,320,.75)*3==local.X);
    assert(osm::preview_axis(200,320,.75)*3==local.X+local.Width);
    D3DVIEWPORT8 ultra={0,0,3840,1080,0,1};
    auto wide=osm::preview_viewport(ultra,1920,540,.375,1);
    assert(wide.X==1200&&wide.Width==1440);
    assert(osm::preview_axis(30,320,1)==30);
    assert(osm::preview_axis(30,320,std::numeric_limits<double>::quiet_NaN())==30);
    unsigned depth=0;
    {osm::PreviewScope unrelated(depth,false);assert(!depth);
     {osm::PreviewScope first(depth,true);assert(depth==1);{osm::PreviewScope nested(depth,true);assert(depth==2);}assert(depth==1);}
     assert(!depth);}
    assert(!depth);
}
