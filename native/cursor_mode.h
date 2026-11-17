#pragma once
#include <cstdint>
namespace osm {
class InputCursorMode {
    std::uint32_t serial_=0;
    bool hidden_=false,point_=false;
    int x_=0,y_=0;
public:
    bool hidden()const{return hidden_;}
    void update(std::uint32_t serial,bool controller_active,bool focused,bool inside) {
        if(!focused||!inside)hidden_=false;
        else if(controller_active&&serial!=serial_)hidden_=true;
        serial_=serial;
    }
    void mouse_moved(int x,int y) {
        if(!point_||x!=x_||y!=y_)hidden_=false;
        point_=true;x_=x;y_=y;
    }
    void release(){hidden_=false;}
};
}
