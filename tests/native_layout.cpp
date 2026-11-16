#include "../native/layout.h"
#include <cassert>
int main(){
 auto a=osm::fit_rect(1920,1080,4.0/3);assert(a.x==240&&a.y==0&&a.width==1440&&a.height==1080);
 auto b=osm::fit_rect(3840,2160,32.0/9);assert(b.x==0&&b.y==540&&b.width==3840&&b.height==1080);
 auto c=osm::fit_rect(1920,1080,1);assert(c.x==420&&c.width==1080&&c.height==1080);
 auto d=osm::fit_rect(0,1080,1);assert(d.width==0);
}
