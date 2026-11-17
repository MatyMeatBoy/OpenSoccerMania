#include "../native/input_proxy.cpp"
#include <cassert>
static unsigned callbacks=0;
static BOOL CALLBACK count(LPCDIDEVICEINSTANCEA,LPVOID){++callbacks;return DIENUM_CONTINUE;}
static HRESULT WINAPI original(void*,DWORD,LPDIENUMDEVICESCALLBACKA callback,LPVOID context,DWORD){DIDEVICEINSTANCEA info={};info.dwSize=sizeof(info);info.dwDevType=DI8DEVTYPE_GAMEPAD;callback(&info,context);return DI_OK;}
struct LatePage {HANDLE mapping=nullptr;osm::PadPage* page=nullptr;};
static DWORD WINAPI late_producer(LPVOID data){
    auto& state=*static_cast<LatePage*>(data);Sleep(50);
    char name[96];std::snprintf(name,sizeof(name),"Local\\OpenSoccerMania-Pads-%lu",GetCurrentProcessId());
    state.mapping=CreateFileMappingA(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,160,name);assert(state.mapping);
    state.page=static_cast<osm::PadPage*>(MapViewOfFile(state.mapping,FILE_MAP_WRITE,0,0,160));assert(state.page);
    *state.page={};state.page->version=1;state.page->sequence=2;state.page->flags=3;
    for(unsigned i=0;i<4;++i)state.page->pads[i].connected=1;
    MemoryBarrier();state.page->magic=osm::pad_magic;return 0;
}
int main(int argc,char**){
    original_enum=original;LatePage late;HANDLE thread=nullptr;
    if(argc>1)thread=CreateThread(nullptr,0,late_producer,&late,0,nullptr);
    const auto begin=GetTickCount64();assert(enum_devices(nullptr,DI8DEVCLASS_GAMECTRL,count,nullptr,0)==DI_OK);
    assert(GetTickCount64()-begin<1000);
    if(thread){assert(callbacks==4&&pad_route==1);WaitForSingleObject(thread,1000);CloseHandle(thread);UnmapViewOfFile(late.page);CloseHandle(late.mapping);std::puts("Late SDL producer discovered before four-pad enumeration");}
    else {assert(callbacks==1&&pad_route==-1);std::puts("Absent SDL producer preserves physical DirectInput fallback within bounded startup wait");}
}
