#pragma once
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <cstdio>
namespace osm {
constexpr DWORD pad_magic=0x504d534f;
struct PadState {DWORD identity,connected;LONG x,y;DWORD buttons,reserved[3];};
struct PadPage {DWORD magic,version,sequence,flags,reserved[4];PadState pads[4];};
static_assert(sizeof(PadState)==32&&sizeof(PadPage)==160,"Four independent controller protocol");
class PadReader {
    HANDLE mapping_=nullptr;const unsigned char* memory_=nullptr;
    ULONGLONG retry_=0,last_change_=0;DWORD sequence_=0;bool seen_=false;PadPage cached_={};
    bool cached(ULONGLONG now,PadPage& out)const {if(!seen_||now-last_change_>500)return false;out=cached_;return true;}
    void close(){if(memory_){UnmapViewOfFile(memory_);memory_=nullptr;}if(mapping_){CloseHandle(mapping_);mapping_=nullptr;}seen_=false;}
public:
    ~PadReader(){close();}
    bool snapshot(PadPage& out) {
        const auto now=GetTickCount64();
        if(!memory_&&now>=retry_) {
            retry_=now+100;
            char name[96];std::snprintf(name,sizeof(name),"Local\\OpenSoccerMania-Pads-%lu",GetCurrentProcessId());
            mapping_=OpenFileMappingA(FILE_MAP_READ,FALSE,name);
            if(mapping_)memory_=static_cast<const unsigned char*>(MapViewOfFile(mapping_,FILE_MAP_READ,0,0,sizeof(PadPage)));
            if(mapping_&&!memory_){CloseHandle(mapping_);mapping_=nullptr;}
        }
        if(!memory_)return false;
        const auto* words=reinterpret_cast<const volatile DWORD*>(memory_);
        const DWORD sequence=words[2];
        if(words[0]!=pad_magic||words[1]!=1){close();return false;}
        if(sequence&1)return cached(now,out);
        std::memcpy(&out,memory_,sizeof(out));MemoryBarrier();
        if(words[2]!=sequence)return cached(now,out);
        if(!seen_||sequence!=sequence_){sequence_=sequence;last_change_=now;seen_=true;}
        if(now-last_change_>500)return false;
        cached_=out;return true;
    }
    bool read(unsigned slot,PadState& state,bool* gameplay=nullptr) {
        PadPage page={};state={};
        if(slot>=4||!snapshot(page)||!page.pads[slot].connected)return false;
        state=page.pads[slot];
        if(gameplay)*gameplay=(page.flags&2)!=0;
        if(!(page.flags&1)){state.x=state.y=0;state.buttons=0;}
        return true;
    }
};
inline GUID pad_guid(unsigned slot){return {0x4f534d50+slot,0x5344,0x4c33,{0x90,0x11,0x50,0x41,0x44,0x00,0x00,0x01}};}
inline int pad_guid_slot(REFGUID guid){for(unsigned i=0;i<4;++i){const GUID wanted=pad_guid(i);if(!std::memcmp(&guid,&wanted,sizeof(guid)))return int(i);}return -1;}
} // namespace osm
