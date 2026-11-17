#include "../native/layout.h"
#include <cassert>
int main(){
 auto a=osm::fit_rect(1920,1080,4.0/3);assert(a.x==240&&a.y==0&&a.width==1440&&a.height==1080);
 auto b=osm::fit_rect(3840,2160,32.0/9);assert(b.x==0&&b.y==540&&b.width==3840&&b.height==1080);
 auto c=osm::fit_rect(1920,1080,1);assert(c.x==420&&c.width==1080&&c.height==1080);
 auto d=osm::fit_rect(0,1080,1);assert(d.width==0);
 auto stock=osm::content_rect({0,0,1920,1080},640,480,false);
 assert(stock.x==240&&stock.y==0&&stock.width==1440&&stock.height==1080);
 auto square=osm::content_rect(c,640,480,false);
 assert(square.x==420&&square.y==135&&square.width==1080&&square.height==810);
 auto full=osm::present_rect({0,0,640,480},2.25f,2.25f,240,0);
 assert(full.x==240&&full.y==0&&full.width==1440&&full.height==1080);
 auto part=osm::present_rect({160,120,320,240},2.25f,2.25f,240,0);
 assert(part.x==600&&part.y==270&&part.width==720&&part.height==540);
}
