#include "../native/input_page.h"
#include <cassert>
#include <cstring>
int main() {
    char name[96];std::snprintf(name,sizeof(name),"Local\\OpenSoccerMania-Input-%lu",GetCurrentProcessId());
    HANDLE map=CreateFileMappingA(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,272,name);
    auto* data=static_cast<unsigned char*>(MapViewOfFile(map,FILE_MAP_ALL_ACCESS,0,0,272));assert(data);
    std::memset(data,0,272);auto* words=reinterpret_cast<DWORD*>(data);
    words[0]=0x494d534f;words[1]=2;words[2]=1;data[16+MapVirtualKeyW(VK_SPACE,0)]=0x80;
    {
        osm::PrivateKeyboard keyboard;
        assert(keyboard.state(VK_SPACE,false)==0); // Production input never reaches an unfocused game.
        assert(static_cast<unsigned short>(keyboard.state(VK_SPACE,true))==0x8001);
        assert(static_cast<unsigned short>(keyboard.state(VK_SPACE,true))==0x8000);
        words[1]=3;assert(keyboard.state(VK_SPACE,true)==0); // Reject a writer in progress.
        words[1]=4;words[2]=3;assert((keyboard.state(VK_SPACE,false)&0x8000)!=0); // Explicit private test mode.
        assert(keyboard.background_test_enabled());
        words[1]=5;assert(keyboard.state(VK_SPACE,true)==0);
        assert(keyboard.background_test_enabled()); // In-progress writes cannot release private ownership.
        words[1]=6;
        words[2]=0;assert(keyboard.state(VK_SPACE,true)==0);
        assert(!keyboard.background_test_enabled());
        data[16+MapVirtualKeyW(VK_ESCAPE,0)]=0x80;words[2]=17;
        assert(keyboard.state(VK_ESCAPE,true,false)==0); // A stale skip cannot back out of the next menu.
        assert((keyboard.state(VK_ESCAPE,true,true)&0x8000)!=0);
        assert(keyboard.state(VK_ESCAPE,false,true)==0);
        words[2]=0x205;assert(keyboard.controller_family()==2);
        words[1]=7;assert(keyboard.controller_family()==1);
        words[1]=8;words[2]=0x405;assert(keyboard.controller_family()==4);
        words[2]=0x705;assert(keyboard.controller_family()==1);
        words[2]=0x400;assert(keyboard.controller_family()==1);
 // Movie skip still respects foreground ownership.
    }
    UnmapViewOfFile(data);CloseHandle(map);
}
