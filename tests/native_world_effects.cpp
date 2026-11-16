#include "../native/world_effects.h"
#include "../native/aspect.h"
#include "../native/screen_vertices.h"
#include <cassert>
#include <vector>
int main() {
    osm::WorldStencilVertex quad[]={{0,0,0,1,0x40000000},{639,0,0,1,0x40000000},{0,479,0,1,0x40000000},{639,479,0,1,0x40000000}};
    auto mask=[&](bool verified=true,std::uintptr_t source=0x65ba20,bool stencil=true){return osm::world_stencil_composite(verified,source,0x44,5,2,20,stencil,quad,640,480);};
    assert(mask());assert(!mask(false));assert(!mask(true,0x65ba24));assert(!mask(true,0x65ba20,false));
    // Identical screen-space depth is not sufficient to turn ordinary UI into
    // a world effect: the exact supported source and resolve pattern matter.
    assert(osm::overlay_depth(quad[0].z,quad[0].rhw));
    quad[1].x=400;assert(!mask());quad[1].x=639;
    quad[2].color=0x40ffffff;assert(!mask());quad[2].color=0x40000000;
    quad[3].rhw=.5f;assert(!mask());quad[3].rhw=1;
    assert(!osm::world_stencil_composite(true,0x65ba20,0x144,5,2,20,true,quad,640,480));
    assert(!osm::world_stencil_composite(true,0x65ba20,0x44,4,2,20,true,quad,640,480));
    assert(!osm::world_stencil_signature(nullptr,0,true));
    std::vector<unsigned char> image(0x25ba70,0);assert(!osm::world_stencil_signature(image.data(),image.size(),true));
    const unsigned char entry[]={0xa1,0x00,0xb8,0x65,0x00,0x85,0xc0,0x0f,0x84,0x1d,0x01,0x00,0x00};
    const unsigned char draw[]={0x6a,0x44,0x50,0xff,0x91,0x30,0x01,0x00,0x00,0xa1,0x28,0x6c,0x61,0x00,0x6a,0x14,0x68,0x20,0xba,0x65,0x00,0x6a,0x02,0x8b,0x10,0x6a,0x05,0x50,0xff,0x92,0x20,0x01,0x00,0x00};
    std::memcpy(image.data()+0xc9100,entry,sizeof(entry));std::memcpy(image.data()+0xc91ee,draw,sizeof(draw));
    assert(osm::world_stencil_signature(image.data(),image.size(),true));
    assert(!osm::world_stencil_signature(image.data(),image.size(),false));
    assert(!osm::world_stencil_signature(image.data(),image.size()-1,true));
    image[0xc920f]^=1;assert(!osm::world_stencil_signature(image.data(),image.size(),true));
    // HUD centering would wrongly restrict this world resolve to the old 4:3
    // center. Its original X span must survive before physical raster scaling.
    const float hud_left=osm::hud_x(quad[0].x,0,640,.75f);
    assert(hud_left>79&&quad[0].x==0&&quad[1].x==639);
    osm::world_stencil_edges(quad,640,480);
    assert(quad[0].x==-.5f&&quad[0].y==-.5f&&quad[3].x==639.5f&&quad[3].y==479.5f);
    assert(osm::raster_coordinate(quad[0].x,3,0)==-.5f);
    assert(osm::raster_coordinate(quad[3].x,3,0)==1919.5f);
    assert(quad[0].color==0x40000000&&quad[0].rhw==1&&quad[0].z==0);
}
