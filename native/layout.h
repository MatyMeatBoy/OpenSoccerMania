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
inline FitRect content_rect(const FitRect& canvas,int logical_width,int logical_height,bool widescreen) {
    if(widescreen)return canvas;
    if(logical_width<=0||logical_height<=0)return {0,0,0,0};
    auto content=fit_rect(canvas.width,canvas.height,static_cast<double>(logical_width)/logical_height);
    content.x+=canvas.x;content.y+=canvas.y;return content;
}
inline FitRect present_rect(const FitRect& logical,float sx,float sy,float ox,float oy) {
    const int left=static_cast<int>(std::lround(ox+logical.x*sx));
    const int top=static_cast<int>(std::lround(oy+logical.y*sy));
    return {left,top,static_cast<int>(std::lround(ox+(logical.x+logical.width)*sx))-left,
                     static_cast<int>(std::lround(oy+(logical.y+logical.height)*sy))-top};
}
}
