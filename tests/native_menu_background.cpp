#define CINTERFACE
#define COBJMACROS
#include "../native/menu_background.h"
#include <cassert>
#include <limits>
int main() {
    std::array<osm::MenuTile,80> tiles;
    for(unsigned y=0;y<8;++y)for(unsigned x=0;x<10;++x)tiles[y*10+x]={reinterpret_cast<void*>(1+y*10+x),float(x*64),float(y*64),3,0x80000000|(y<<8)|x};
    const auto original=tiles;std::array<osm::MenuSideTile,1024> output;
    const osm::MenuClip clip{0,0,640,480};
    auto count=osm::menu_side_tiles(tiles.data(),tiles.size(),clip,.75f,output.data(),output.size());
    assert(count==32);
    unsigned left=0,right=0;
    for(std::size_t i=0;i<count;++i) {
        const auto& t=output[i].tile;const auto side=output[i].side;
        const int column=((static_cast<int>(t.x)/64)%10+10)%10,row=static_cast<int>(t.y)/64;
        assert(t.sprite==original[row*10+column].sprite&&t.color==original[row*10+column].color&&t.flags==3);
        assert(t.x<0||t.x>=640);if(side){++right;assert(t.x==640||t.x==704);}else{++left;assert(t.x==-128||t.x==-64);}
    }
    assert(left==16&&right==16&&std::memcmp(tiles.data(),original.data(),sizeof(tiles))==0);
    assert(osm::menu_side_tiles(tiles.data(),80,clip,.375f,output.data(),output.size())==144);
    assert(!osm::menu_side_tiles(tiles.data(),80,clip,1,output.data(),output.size()));
    assert(!osm::menu_side_tiles(tiles.data(),80,clip,std::numeric_limits<float>::quiet_NaN(),output.data(),output.size()));
    assert(!osm::menu_side_tiles(tiles.data(),79,clip,.75f,output.data(),output.size()));
    assert(!osm::menu_side_tiles(tiles.data(),80,clip,.75f,output.data(),1));
    tiles[1].x=0;assert(!osm::menu_side_tiles(tiles.data(),80,clip,.75f,output.data(),output.size()));tiles=original;
    tiles[0].flags=1;assert(!osm::menu_side_tiles(tiles.data(),80,clip,.75f,output.data(),output.size()));
    assert(osm::menu_prompt_context(0x4f72d1)==1&&osm::menu_prompt_context(0x4f7327)==2);
    assert(!osm::menu_prompt_context(0x52f45e)&&!osm::menu_prompt_context(0x4f72d0));
    const auto a=osm::menu_button_pixels(false),b=osm::menu_button_pixels(true);
    assert(a.size()==4096&&b.size()==4096&&a!=b&&a[0]==0&&b[4095]==0);
    const DWORD green=a[32*64+18],red=b[32*64+18];
    assert((green>>24)==255&&((green>>8)&255)>((green>>16)&255));
    assert((red>>24)==255&&((red>>16)&255)>((red>>8)&255));
    assert((a[23*64+31]&0xffffff)==0xf5f5f5&&(b[30*64+25]&0xffffff)==0xf5f5f5);
    assert(!osm::menu_background_signature(nullptr,0,true));
    std::vector<unsigned char> image(0x12f4a0,0);assert(!osm::menu_background_signature(image.data(),image.size(),true));
    const auto put=[&](unsigned offset,std::initializer_list<unsigned char> bytes){std::copy(bytes.begin(),bytes.end(),image.begin()+offset);};
    put(0xe41a0,{0x81,0xec,0x84,0,0,0,0x8b,0x84,0x24,0x88,0,0,0});
    put(0x12f3d0,{0x83,0xec,0x0c,0x53,0x55,0x56,0x8b,0x74,0x24,0x1c});
    put(0x12f459,{0xe8,0x42,0x4d,0xfb,0xff});put(0xf72cc,{0xe8,0xcf,0xce,0xfe,0xff});
    put(0xf7322,{0xe8,0x79,0xce,0xfe,0xff});put(0x15410,{0x6a,1,0xe8,0xa9,0x0c,0,0});
    assert(osm::menu_background_signature(image.data(),image.size(),true));
    assert(!osm::menu_background_signature(image.data(),image.size(),false));
    assert(!osm::menu_background_signature(image.data(),image.size()-1,true));
    for(unsigned offset:{0xe41a0,0x12f3d0,0x12f459,0xf72cc,0xf7322,0x15410}) {
        image[offset]^=1;assert(!osm::menu_background_signature(image.data(),image.size(),true));image[offset]^=1;
    }
}
