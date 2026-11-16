#pragma once
#include <algorithm>
#include <cmath>
namespace osm {
struct FitRect{int x,y,width,height;};
inline FitRect fit_rect(int width,int height,double aspect) {
    if(width<=0||height<=0||aspect<=0)return {0,0,0,0};
    int w=width,h=static_cast<int>(std::lround(width/aspect));
    if(h>height){h=height;w=static_cast<int>(std::lround(height*aspect));}
    w=std::clamp(w,1,width);h=std::clamp(h,1,height);
    return {(width-w)/2,(height-h)/2,w,h};
}
}
