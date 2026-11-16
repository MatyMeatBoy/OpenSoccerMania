#include "../native/input_proxy.cpp"
#include <cassert>
#include <cstdio>
#include <initializer_list>

static unsigned real_polls=0;
static HRESULT WINAPI fake_state(void*,DWORD size,LPVOID data) {
    ++real_polls;
    if(data)std::memset(data,0x55,size);
    return DIERR_INPUTLOST;
}

int main() {
    char name[96];osm::ui_state_name(name);
    osm::UiStateReader reader;
    assert(!reader.panel_open());
    // Readers must not create a mapping that prevents the native publisher.
    HANDLE absent=OpenFileMappingA(FILE_MAP_READ,FALSE,name);
    assert(!absent);
    HANDLE observer=nullptr;
    const osm::UiStatePage* view=nullptr;
    {
        osm::UiStatePublisher publisher;
        assert(publisher.open());
        original_state=fake_state;
        publisher.publish(true,false);
        unsigned char packet[sizeof(DIJOYSTATE2)]={};
        for(DWORD size:{DWORD(256),DWORD(sizeof(DIMOUSESTATE)),DWORD(sizeof(DIMOUSESTATE2))}) {
            assert(device_state(nullptr,size,packet)==DI_OK);
            for(DWORD i=0;i<size;++i)assert(packet[i]==0);
        }
        assert(real_polls==3);
        assert(device_state(nullptr,sizeof(DIJOYSTATE2),packet)==DIERR_INPUTLOST);
        assert(packet[0]==0x55); // POV/axis neutrality is not assumed
        assert(device_state(nullptr,256,nullptr)==DIERR_INPUTLOST);
        publisher.publish(false,false);
        assert(device_state(nullptr,sizeof(DIMOUSESTATE),packet)==DIERR_INPUTLOST);
        assert(packet[0]==0x55);
        assert(publisher.open()); // same owner is idempotent
        osm::UiStatePublisher other;
        assert(!other.open()); // no second writer may reset the first
        observer=OpenFileMappingA(FILE_MAP_READ,FALSE,name);
        assert(observer);
        view=static_cast<const osm::UiStatePage*>(MapViewOfFile(observer,FILE_MAP_READ,0,0,32));
        assert(view && view->magic==osm::ui_state_magic && view->version==1);
        publisher.publish(true,true);
        assert(view->flags==3 && view->revision==3);
        Sleep(260); // retry is bounded rather than OpenFileMapping every frame
        assert(reader.panel_open());
        publisher.publish(false,true);
        assert(!reader.panel_open() && view->flags==2 && view->revision==4);
        publisher.publish(true,false);
        assert(reader.panel_open() && view->flags==1);
    }
    assert(view->magic==0 && view->flags==0); // destructor invalidates surviving views
    assert(!reader.panel_open());
    assert(!ui_state.panel_open());
    UnmapViewOfFile(view);CloseHandle(observer);
    HANDLE invalid=CreateFileMappingA(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,32,name);
    assert(invalid && GetLastError()!=ERROR_ALREADY_EXISTS);
    auto* page=static_cast<osm::UiStatePage*>(MapViewOfFile(invalid,FILE_MAP_WRITE,0,0,32));
    assert(page);
    *page={osm::ui_state_magic,99,1,0,{0,0,0,0}};
    Sleep(260);
    assert(!reader.panel_open()); // incompatible version is never trusted
    page->version=1;page->magic=0x12345678;
    Sleep(260);
    assert(!reader.panel_open());
    UnmapViewOfFile(page);CloseHandle(invalid);
    {
        osm::UiStatePublisher replacement;
        assert(replacement.open());
        replacement.publish(true,false);
        Sleep(260);
        assert(reader.panel_open()); // reader reconnects to replacement publisher
        replacement.close();replacement.close();
        assert(!reader.panel_open());
    }
    std::puts("native UI state: ownership, late publication, flags, invalid headers, cleanup passed");
}
