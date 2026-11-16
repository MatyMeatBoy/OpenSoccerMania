#pragma once
#include <algorithm>
#include <array>
#include <vector>

namespace osm {
struct FrameSummary {
    double fps=0,average_ms=0,p95_ms=0,p99_ms=0,max_ms=0,present_ms=0;
    size_t samples=0;
};
class FrameStats {
    std::array<double,300> frames={},waits={};
    size_t next=0,count=0;
public:
    void record(double elapsed_ms,double present_ms) {
        if(elapsed_ms<=0 || elapsed_ms>10000)return;
        frames[next]=elapsed_ms;waits[next]=std::max(0.0,present_ms);
        next=(next+1)%frames.size();count=std::min(count+1,frames.size());
    }
    FrameSummary summary() const {
        FrameSummary value;value.samples=count;if(!count)return value;
        std::vector<double> sorted(frames.begin(),frames.begin()+count);
        double total=0,wait=0;
        for(size_t i=0;i<count;++i){total+=frames[i];wait+=waits[i];}
        std::sort(sorted.begin(),sorted.end());
        value.average_ms=total/count;value.fps=1000.0/value.average_ms;
        value.p95_ms=sorted[(count-1)*95/100];value.p99_ms=sorted[(count-1)*99/100];
        value.max_ms=sorted.back();value.present_ms=wait/count;return value;
    }
    std::vector<double> history() const {
        std::vector<double> result;result.reserve(count);
        const size_t start=count==frames.size()?next:0;
        for(size_t i=0;i<count;++i)result.push_back(frames[(start+i)%frames.size()]);
        return result;
    }
};
}
