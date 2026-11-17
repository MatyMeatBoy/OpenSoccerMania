#pragma once
#include <windows.h>
#include <cstdio>
namespace osm {
// Read-only ownership of explicitly requested PID-local background diagnostics.
// An absent, disabled or unstable packet never weakens normal focus checks.
class PrivateInputDiagnostics {
    HANDLE mapping_=nullptr;const volatile DWORD* words_=nullptr;
    ULONGLONG retry_=0;
public:
    ~PrivateInputDiagnostics(){if(words_)UnmapViewOfFile(const_cast<const DWORD*>(words_));if(mapping_)CloseHandle(mapping_);}
    bool enabled() {
        if(!words_&&GetTickCount64()>=retry_) {
            retry_=GetTickCount64()+100;
            char name[96];std::snprintf(name,sizeof(name),"Local\\OpenSoccerMania-Input-%lu",GetCurrentProcessId());
            mapping_=OpenFileMappingA(FILE_MAP_READ,FALSE,name);
            if(mapping_)words_=static_cast<const volatile DWORD*>(MapViewOfFile(mapping_,FILE_MAP_READ,0,0,272));
            if(mapping_&&!words_){CloseHandle(mapping_);mapping_=nullptr;}
        }
        if(!words_)return false;
        const DWORD sequence=words_[1];
        if(sequence&1)return false;
        const DWORD magic=words_[0],flags=words_[2];MemoryBarrier();
        return words_[1]==sequence&&magic==0x494d534f&&(flags&3)==3;
    }
};
class PrivateKeyboard {
    HANDLE mapping_=nullptr;
    const unsigned char* memory_=nullptr;
    ULONGLONG retry_=0;
    bool down_[256]={};
    mutable bool diagnostic_=false;
public:
    bool background_test_enabled()const {
        if(!memory_)return false;
        const auto* words=reinterpret_cast<const volatile DWORD*>(memory_);
        const DWORD sequence=words[1],flags=words[2];MemoryBarrier();
        if(words[0]==0x494d534f&&!(sequence&1)&&words[1]==sequence)diagnostic_=(flags&3)==3;
        return diagnostic_;
    }
    bool controller_present()const {
        if(!memory_)return false;
        const auto* words=reinterpret_cast<const volatile DWORD*>(memory_);
        return words[0]==0x494d534f&&!(words[1]&1)&&(words[2]&4);
    }
    unsigned controller_family()const {
        if(!memory_)return 1;
        const auto* words=reinterpret_cast<const volatile DWORD*>(memory_);
        const DWORD sequence=words[1],flags=words[2],magic=words[0];MemoryBarrier();
        if(magic!=0x494d534f||(sequence&1)||words[1]!=sequence||!(flags&4))return 1;
        const unsigned family=(flags>>8)&7;
        return family>=1&&family<=4?family:1;
    }
    DWORD controller_activity(bool& active)const {
        active=false;if(!memory_)return 0;
        const auto* words=reinterpret_cast<const volatile DWORD*>(memory_);
        const DWORD sequence=words[1],flags=words[2],serial=words[3];MemoryBarrier();
        if(words[0]!=0x494d534f||(sequence&1)||words[1]!=sequence)return 0;
        active=(flags&9)==9;return serial;
    }
    ~PrivateKeyboard(){if(memory_)UnmapViewOfFile(memory_);if(mapping_)CloseHandle(mapping_);}
    SHORT state(int key,bool focused,bool movie=false) {
        if(key<0||key>255)return 0;
        if(!memory_&&GetTickCount64()>=retry_) {
            retry_=GetTickCount64()+1000;
            char name[96];std::snprintf(name,sizeof(name),"Local\\OpenSoccerMania-Input-%lu",GetCurrentProcessId());
            mapping_=OpenFileMappingA(FILE_MAP_READ,FALSE,name);
            if(mapping_)memory_=static_cast<const unsigned char*>(MapViewOfFile(mapping_,FILE_MAP_READ,0,0,272));
            if(mapping_&&!memory_){CloseHandle(mapping_);mapping_=nullptr;}
        }
        bool pressed=false;
        background_test_enabled(); // Latch ownership even while no private key is down.
        if(memory_) {
            const auto* words=reinterpret_cast<const volatile DWORD*>(memory_);
            const DWORD sequence=words[1],flags=words[2];
            if(words[0]==0x494d534f&&!(sequence&1)&&(flags&1)&&((flags&2)||focused)) {
                UINT scan=MapVirtualKeyW(key,MAPVK_VK_TO_VSC);
                if(key==VK_LEFT||key==VK_RIGHT||key==VK_UP||key==VK_DOWN||key==VK_PRIOR||key==VK_NEXT)scan|=0x80;
                if(scan<256){const bool value=(memory_[16+scan]&0x80)!=0;MemoryBarrier();pressed=words[1]==sequence&&value;}
                if(key==VK_ESCAPE&&(flags&16)&&!movie)pressed=false;
            }
        }
        const bool edge=pressed&&!down_[key];down_[key]=pressed;
        return pressed?static_cast<SHORT>(0x8000|(edge?1:0)):0;
    }
};
}
