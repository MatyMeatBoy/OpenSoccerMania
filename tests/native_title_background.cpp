#define CINTERFACE
#define COBJMACROS
#include "../native/title_background.h"
#include <cassert>
static void put(std::vector<unsigned char>& b,size_t offset,unsigned value){for(int i=0;i<4;++i)b[offset+i]=static_cast<unsigned char>(value>>(8*i));}
int main(){
    osm::TitleSide sides[2];assert(!osm::title_sides(640,480,sides));assert(!osm::title_sides(0,0,sides));
    for(unsigned w:{1280u,1920u,3440u,5120u}){assert(osm::title_sides(w,720,sides));assert(sides[0].x1<=sides[1].x0);assert(sides[0].x0==0);assert(sides[1].x1==w);assert(sides[0].u1<.5f&&sides[1].u0>.5f);}
    assert(osm::title_draw_context(0x4f2f86));assert(osm::title_draw_context(0x4f30a1));assert(!osm::title_draw_context(0x4f2d90));
    std::vector<unsigned char> b(122+2048*512*4,0);b[0]='B';b[1]='M';put(b,2,unsigned(b.size()));put(b,10,122);put(b,14,108);put(b,18,2048);put(b,22,512);b[26]=1;b[28]=32;put(b,30,3);put(b,34,2048*512*4);put(b,54,0xff0000);put(b,58,0xff00);put(b,62,0xff);put(b,66,0xff000000);
    for(unsigned y=0;y<512;++y)for(unsigned x=0;x<2048;++x)if(x<682||x>=1365){b[122+(y*2048+x)*4]=static_cast<unsigned char>(y);b[122+(y*2048+x)*4+3]=255;}
    osm::TitleBmp decoded;assert(osm::decode_title_bmp(b,decoded));assert(decoded.width==2048&&decoded.height==512);assert(decoded.bgra[0]==255);
    b[122+682*4]=1;assert(!osm::decode_title_bmp(b,decoded));b[122+682*4]=0;
    b[122+682*4+3]=255;assert(!osm::decode_title_bmp(b,decoded));b[122+682*4+3]=0;
    b.pop_back();assert(!osm::decode_title_bmp(b,decoded));
    std::vector<unsigned char> image(0xf30a5,0);assert(!osm::title_hook_signature(image.data(),image.size()));assert(!osm::title_hook_signature(nullptr,0));
    return 0;
}
