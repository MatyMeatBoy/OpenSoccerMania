#include "../native/frame_stats.h"
#include <cassert>
#include <cmath>
int main(){
 osm::FrameStats stats;
 for(int i=0;i<120;++i)stats.record(1000.0/60.0,0.5);
 auto value=stats.summary();
 assert(std::fabs(value.fps-60.0)<0.001);
 assert(std::fabs(value.p99_ms-1000.0/60.0)<0.001);
 stats.record(100,20);
 value=stats.summary();assert(value.max_ms==100);assert(value.present_ms>0);
 assert(value.samples==121);
}
