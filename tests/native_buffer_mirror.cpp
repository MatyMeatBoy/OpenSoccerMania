#include "../native/buffer_mirror.h"
#include <cassert>
#include <cstring>
int main() {
    osm::BufferMirror mirror(8);
    assert(!mirror.read(0,8));
    unsigned char initial[]={1,2,3,4,5,6,7,8};
    assert(mirror.begin(0,0,false));mirror.stage(initial);mirror.finish(true);
    initial[0]=99;assert(mirror.read(0,8)[0]==1); // Own the data after Unlock.
    unsigned char partial[]={20,21};
    assert(mirror.begin(2,2,false));assert(!mirror.read(0,8));mirror.stage(partial);mirror.finish(true);
    assert(mirror.read(0,8)[2]==20&&mirror.read(0,8)[4]==5);
    assert(mirror.begin(4,2,true));mirror.stage(partial);mirror.finish(true);
    assert(!mirror.read(0,8));assert(mirror.read(4,2)[1]==21); // DISCARD invalidates untouched bytes.
    assert(mirror.begin(4,2,false));mirror.stage(initial);mirror.finish(false);
    assert(!mirror.read(4,2)); // Failed Unlock must not publish data.
    assert(!mirror.begin(9,0,false));assert(!mirror.begin(7,2,false));
    assert(!mirror.read(7,2));
}
