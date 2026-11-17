#pragma once
#include <windows.h>
#include <cstdio>

namespace osm {
constexpr DWORD ui_state_magic=0x554d534f;
constexpr DWORD ui_state_version=1;
struct UiStatePage {
    DWORD magic,version,flags,revision,reserved[4];
};
static_assert(sizeof(UiStatePage)==32,"UI state protocol size");
inline void ui_state_name(char (&name)[96]) {
    std::snprintf(name,sizeof(name),"Local\\OpenSoccerMania-UI-%lu",GetCurrentProcessId());
}
class UiStatePublisher {
    HANDLE mapping_=nullptr;
    UiStatePage* page_=nullptr;
public:
    UiStatePublisher()=default;
    UiStatePublisher(const UiStatePublisher&)=delete;
    UiStatePublisher& operator=(const UiStatePublisher&)=delete;
    ~UiStatePublisher(){close();}
    bool open() {
        if(page_)return true;
        char name[96];ui_state_name(name);
        mapping_=CreateFileMappingA(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(UiStatePage),name);
        if(!mapping_)return false;
        if(GetLastError()==ERROR_ALREADY_EXISTS){CloseHandle(mapping_);mapping_=nullptr;return false;}
        page_=static_cast<UiStatePage*>(MapViewOfFile(mapping_,FILE_MAP_WRITE,0,0,sizeof(UiStatePage)));
        if(!page_){CloseHandle(mapping_);mapping_=nullptr;return false;}
        *page_={0,ui_state_version,0,0,{0,0,0,0}};
        MemoryBarrier();
        InterlockedExchange(reinterpret_cast<volatile LONG*>(&page_->magic),ui_state_magic);
        return true;
    }
    void publish(bool panel_open,bool gameplay,bool movie=false) {
        if(!page_)return;
        InterlockedExchange(reinterpret_cast<volatile LONG*>(&page_->flags),(panel_open?1:0)|(gameplay?2:0)|(movie?4:0));
        InterlockedIncrement(reinterpret_cast<volatile LONG*>(&page_->revision));
    }
    void close() {
        if(page_) {
            InterlockedExchange(reinterpret_cast<volatile LONG*>(&page_->flags),0);
            InterlockedExchange(reinterpret_cast<volatile LONG*>(&page_->magic),0);
            UnmapViewOfFile(page_);page_=nullptr;
        }
        if(mapping_){CloseHandle(mapping_);mapping_=nullptr;}
    }
};
class UiStateReader {
    HANDLE mapping_=nullptr;
    const volatile UiStatePage* page_=nullptr;
    ULONGLONG next_attempt_=0;
    void release() {
        if(page_){UnmapViewOfFile(const_cast<const UiStatePage*>(page_));page_=nullptr;}
        if(mapping_){CloseHandle(mapping_);mapping_=nullptr;}
    }
public:
    UiStateReader()=default;
    UiStateReader(const UiStateReader&)=delete;
    UiStateReader& operator=(const UiStateReader&)=delete;
    ~UiStateReader(){release();}
    DWORD flags() {
        if(!page_) {
            const auto now=GetTickCount64();
            if(now<next_attempt_)return false;
            next_attempt_=now+250;
            char name[96];ui_state_name(name);
            mapping_=OpenFileMappingA(FILE_MAP_READ,FALSE,name);
            if(!mapping_)return false;
            page_=static_cast<const volatile UiStatePage*>(MapViewOfFile(mapping_,FILE_MAP_READ,0,0,sizeof(UiStatePage)));
            if(!page_){release();return false;}
        }
        if(page_->magic!=ui_state_magic || page_->version!=ui_state_version){release();return false;}
        const DWORD flags=page_->flags;
        MemoryBarrier();
        if(page_->magic!=ui_state_magic || page_->version!=ui_state_version){release();return false;}
        return flags;
    }
    bool panel_open(){return (flags()&1)!=0;}
    bool movie_active(){return (flags()&4)!=0;}
};
} // namespace osm
