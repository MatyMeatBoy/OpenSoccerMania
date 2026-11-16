#pragma once
#include <windows.h>
#include <cstdio>
namespace osm {
class PrivateKeyboard {
    HANDLE mapping_=nullptr;
    const unsigned char* memory_=nullptr;
    ULONGLONG retry_=0;
    bool down_[256]={};
public:
    ~PrivateKeyboard(){if(memory_)UnmapViewOfFile(memory_);if(mapping_)CloseHandle(mapping_);}
    SHORT state(int key,bool focused) {
        if(key<0||key>255)return 0;
        if(!memory_&&GetTickCount64()>=retry_) {
            retry_=GetTickCount64()+1000;
            char name[96];std::snprintf(name,sizeof(name),"Local\\OpenSoccerMania-Input-%lu",GetCurrentProcessId());
            mapping_=OpenFileMappingA(FILE_MAP_READ,FALSE,name);
            if(mapping_)memory_=static_cast<const unsigned char*>(MapViewOfFile(mapping_,FILE_MAP_READ,0,0,272));
            if(mapping_&&!memory_){CloseHandle(mapping_);mapping_=nullptr;}
        }
        bool pressed=false;
        if(memory_) {
            const auto* words=reinterpret_cast<const volatile DWORD*>(memory_);
            const DWORD sequence=words[1],flags=words[2];
            if(words[0]==0x494d534f&&!(sequence&1)&&(flags&1)&&((flags&2)||focused)) {
                UINT scan=MapVirtualKeyW(key,MAPVK_VK_TO_VSC);
                if(key==VK_LEFT||key==VK_RIGHT||key==VK_UP||key==VK_DOWN)scan|=0x80;
                if(scan<256){const bool value=(memory_[16+scan]&0x80)!=0;MemoryBarrier();pressed=words[1]==sequence&&value;}
            }
        }
        const bool edge=pressed&&!down_[key];down_[key]=pressed;
        return pressed?static_cast<SHORT>(0x8000|(edge?1:0)):0;
    }
};
}
