#pragma once
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <cstring>
namespace osm {
class SnapshotHistory {
    bool pending_=false;
public:
    void reset(){pending_=true;}
    // A newly allocated previous bank has no simulation endpoint yet. Seed it
    // from the first complete save, before any render can feed zeros back into
    // persistent fields such as the camera's up vector and aspect ratio.
    bool prime(unsigned char* current,unsigned char* previous,std::size_t bytes) {
        if(!pending_||!current||!previous||!bytes||bytes>16*1024*1024)return false;
        const auto a=reinterpret_cast<std::uintptr_t>(current),b=reinterpret_cast<std::uintptr_t>(previous);
        if((a>b?a-b:b-a)<bytes)return false;
        std::memcpy(previous,current,bytes);pending_=false;return true;
    }
};
using PoseInterpolate=int (*)(void*,void*,void*,float);
struct InterpolationNode {
    void* object;
    void* save;
    PoseInterpolate interpolate;
    InterpolationNode* next;
};
// The legacy dispatcher substitutes 1 near a tick boundary. At independent
// render rates this briefly presents the endpoint, then rewinds on the next
// fraction. Both zero and small fractions must remain valid interpolation.
inline bool valid_interpolation_fraction(float alpha) {
    return std::isfinite(alpha)&&alpha>=0.f&&alpha<=1.f;
}
inline bool interpolate_snapshots(InterpolationNode* node,unsigned char* current,
                                  unsigned char* previous,float alpha) {
    if(!valid_interpolation_fraction(alpha)||!current||!previous)return false;
    for(unsigned count=0;node&&count<4096;++count) {
        if(!node->interpolate)return false;
        const int words=node->interpolate(node->object,current,previous,alpha);
        if(words<0||words>1048576)return false;
        current+=static_cast<std::size_t>(words)*4;
        previous+=static_cast<std::size_t>(words)*4;
        node=node->next;
    }
    return !node;
}
}
