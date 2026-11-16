#pragma once
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <vector>
namespace osm {
class BufferMirror {
    std::vector<unsigned char> bytes_,valid_;
    std::size_t offset_=0,length_=0;
    bool pending_=false;
public:
    explicit BufferMirror(std::size_t size):bytes_(size),valid_(size){}
    std::size_t size()const{return bytes_.size();}
    bool begin(std::size_t offset,std::size_t length,bool discard) {
        if(offset>size())return false;
        if(!length)length=size()-offset;
        if(length>size()-offset)return false;
        if(discard)std::fill(valid_.begin(),valid_.end(),0);
        offset_=offset;length_=length;pending_=true;
        std::fill(valid_.begin()+offset,valid_.begin()+offset+length,0);return true;
    }
    void stage(const void* source){if(pending_&&source)std::memcpy(bytes_.data()+offset_,source,length_);}
    void finish(bool success){if(pending_){std::fill(valid_.begin()+offset_,valid_.begin()+offset_+length_,success?1:0);pending_=false;}}
    const unsigned char* read(std::size_t offset,std::size_t length)const {
        if(offset>size()||length>size()-offset)return nullptr;
        if(!std::all_of(valid_.begin()+offset,valid_.begin()+offset+length,[](unsigned char v){return v!=0;}))return nullptr;
        return bytes_.data()+offset;
    }
};
}
