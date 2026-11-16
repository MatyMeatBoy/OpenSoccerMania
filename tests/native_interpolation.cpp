#include "../native/interpolation.h"
#include <cassert>
#include <cstring>
#include <limits>
#include <initializer_list>
struct Pose {float x;unsigned words;};
static int blend(void* object,void* current,void* previous,float alpha) {
    auto* pose=static_cast<Pose*>(object);float a,b;
    std::memcpy(&a,current,4);std::memcpy(&b,previous,4);
    pose->x=b+(a-b)*alpha;return static_cast<int>(pose->words);
}
int main() {
    // Two differently sized snapshot records catch incorrect bank strides.
    float current[]={20,99,40},previous[]={10,88,30};
    Pose a{0,2},b{0,1};
    osm::InterpolationNode second{&b,nullptr,blend,nullptr},first{&a,nullptr,blend,&second};
    float last_a=10,last_b=30;
    // Reproduce the old boundary jump: endpoint at zero, then an earlier pose.
    blend(&a,current,previous,1.f);const float snapped=a.x;
    blend(&a,current,previous,.25f);assert(a.x<snapped);
    for(float alpha:{0.f,.01f,.049f,.05f,.051f,.25f,.75f,1.f}) {
        assert(osm::interpolate_snapshots(&first,reinterpret_cast<unsigned char*>(current),reinterpret_cast<unsigned char*>(previous),alpha));
        assert(a.x>=last_a&&b.x>=last_b);last_a=a.x;last_b=b.x;
        assert(current[0]==20&&previous[0]==10&&current[1]==99&&previous[1]==88);
    }
    // A tick boundary changes the bank endpoints; the pose remains continuous.
    previous[0]=20;current[0]=30;
    assert(osm::interpolate_snapshots(&first,reinterpret_cast<unsigned char*>(current),reinterpret_cast<unsigned char*>(previous),0));
    assert(a.x==20);
    assert(!osm::valid_interpolation_fraction(-.1f));
    assert(!osm::valid_interpolation_fraction(std::numeric_limits<float>::quiet_NaN()));
}
