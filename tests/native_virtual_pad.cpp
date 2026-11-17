#include "../native/input_proxy.cpp"
#include <cassert>
#include <vector>

struct PadPublisher {
    HANDLE mapping;osm::PadPage* page;
    PadPublisher(){char name[96];std::snprintf(name,sizeof(name),"Local\\OpenSoccerMania-Pads-%lu",GetCurrentProcessId());mapping=CreateFileMappingA(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,160,name);assert(mapping);page=static_cast<osm::PadPage*>(MapViewOfFile(mapping,FILE_MAP_WRITE,0,0,160));assert(page);*page={};page->magic=osm::pad_magic;page->version=1;page->flags=3;}
    void commit(){page->sequence+=2;MemoryBarrier();}
    ~PadPublisher(){page->flags=0;page->magic=0;UnmapViewOfFile(page);CloseHandle(mapping);}
};
static BOOL CALLBACK collect(LPCDIDEVICEINSTANCEA info,LPVOID context){static_cast<std::vector<GUID>*>(context)->push_back(info->guidInstance);return DIENUM_CONTINUE;}
static HRESULT WINAPI enumerate_physical(void*,DWORD,LPDIENUMDEVICESCALLBACKA callback,LPVOID context,DWORD){
    DIDEVICEINSTANCEA info={};info.dwSize=sizeof(info);info.dwDevType=DI8DEVTYPE_KEYBOARD;info.guidInstance.Data1=1;if(callback(&info,context)==DIENUM_STOP)return DI_OK;
    info.dwDevType=DI8DEVTYPE_MOUSE;info.guidInstance.Data1=2;if(callback(&info,context)==DIENUM_STOP)return DI_OK;
    info.dwDevType=DI8DEVTYPE_GAMEPAD;info.guidInstance.Data1=100;callback(&info,context);return DI_OK;
}
static HRESULT WINAPI physical_keyboard(void*,DWORD size,LPVOID data){if(size==256&&data){std::memset(data,0,256);static_cast<BYTE*>(data)[DIK_W]=0x80;}return DI_OK;}
int main(int argc,char** argv){
    PadPublisher publisher;
    for(unsigned i=0;i<4;++i){publisher.page->pads[i]={100+i,1,i==0?32767:i==1?-32767:0,i==2?-32767:i==3?32767:0,1u<<i,{0,0,0}};}publisher.commit();
    original_enum=enumerate_physical;
    std::vector<GUID> ids;assert(enum_devices(nullptr,DI8DEVCLASS_ALL,collect,&ids,0)==DI_OK);
    assert(ids.size()==6&&ids[0].Data1==1&&ids[1].Data1==2);
    for(unsigned i=0;i<4;++i)assert(osm::pad_guid_slot(ids[2+i])==int(i));
    // Public standard DIJOYSTATE2 shape, also verified at the game's pad format
    // (272 bytes/164 objects). The adjacent 20-byte format is its MOUSE format.
    GUID axes[]={osm::pad_object_guid(0xa36d02e0),osm::pad_object_guid(0xa36d02e1),osm::pad_object_guid(0xa36d02e2),osm::pad_object_guid(0xa36d02f4),osm::pad_object_guid(0xa36d02f5),osm::pad_object_guid(0xa36d02e3),osm::pad_object_guid(0xa36d02e4),osm::pad_object_guid(0xa36d02e4)};
    std::vector<DIOBJECTDATAFORMAT> objects;
    for(unsigned i=0;i<8;++i)objects.push_back({&axes[i],i*4,DIDFT_OPTIONAL|DIDFT_AXIS|DIDFT_ANYINSTANCE,DIDOI_ASPECTPOSITION});
    for(unsigned i=0;i<4;++i)objects.push_back({nullptr,DWORD(DIJOFS_POV(i)),DIDFT_OPTIONAL|DIDFT_POV|DIDFT_ANYINSTANCE,0});
    for(unsigned i=0;i<128;++i)objects.push_back({nullptr,DWORD(DIJOFS_BUTTON(i)),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0});
    for(unsigned aspect=2;aspect<=4;++aspect)for(unsigned i=0;i<8;++i)objects.push_back({&axes[i],176+(aspect-2)*32+i*4,DIDFT_OPTIONAL|DIDFT_AXIS|DIDFT_ANYINSTANCE,aspect*0x100});
    assert(objects.size()==164);
    DIDATAFORMAT format={sizeof(format),sizeof(DIOBJECTDATAFORMAT),DIDF_ABSAXIS,sizeof(DIJOYSTATE2),DWORD(objects.size()),objects.data()};
    IDirectInputDevice8A* pads[4]={};
    for(unsigned i=0;i<4;++i){void* object=nullptr;const GUID guid=osm::pad_guid(i);assert(create_device(nullptr,guid,&object,nullptr)==DI_OK);pads[i]=static_cast<IDirectInputDevice8A*>(object);assert(pads[i]->SetDataFormat(&format)==DI_OK);assert(pads[i]->Acquire()==DI_OK);}
    for(unsigned i=0;i<4;++i){DIJOYSTATE2 state={};assert(pads[i]->GetDeviceState(sizeof(state),&state)==DI_OK);assert(state.lX==(i==0?1000:i==1?-1000:0));assert(state.lY==(i==2?-1000:i==3?1000:0));assert(state.rgdwPOV[0]==0xffffffff&&state.lVX==0&&state.lVY==0&&state.lAX==0&&state.lFY==0);for(unsigned button=0;button<32;++button)assert(state.rgbButtons[button]==(button==(i==1?2:i==2?1:i)?0x80:0));}
    const DWORD stable_sequence=publisher.page->sequence;publisher.page->sequence=stable_sequence|1;
    DIJOYSTATE2 during_write={};assert(pads[0]->GetDeviceState(sizeof(during_write),&during_write)==DI_OK&&during_write.lX==1000);
    publisher.page->sequence=stable_sequence;
    publisher.page->pads[0].buttons=(1u<<9)|(1u<<10)|(1u<<6);publisher.commit();
    DIJOYSTATE2 state={};assert(pads[0]->GetDeviceState(sizeof(state),&state)==DI_OK);assert(state.rgbButtons[6]==0x80&&state.rgbButtons[7]==0x80&&state.rgbButtons[8]==0x80);
    DIPROPRANGE x_range={{sizeof(DIPROPRANGE),sizeof(DIPROPHEADER),DIJOFS_X,DIPH_BYOFFSET},0,65535};
    DIPROPRANGE y_range={{sizeof(DIPROPRANGE),sizeof(DIPROPHEADER),DIJOFS_Y,DIPH_BYOFFSET},-100,100};
    assert(pads[0]->SetProperty(DIPROP_RANGE,&x_range.diph)==DI_OK);
    assert(pads[0]->SetProperty(DIPROP_RANGE,&y_range.diph)==DI_OK);
    assert(pads[0]->GetDeviceState(sizeof(state),&state)==DI_OK&&state.lX==65535&&state.lY==0);
    // Internal settings must consume controller input and held buttons must not
    // leak into gameplay when the panel closes.
    osm::UiStatePublisher ui;assert(ui.open());ui.publish(true,true);Sleep(260);
    assert(pads[0]->GetDeviceState(sizeof(state),&state)==DI_OK&&state.lX==32767&&state.lY==0);
    for(auto button:state.rgbButtons)assert(button==0);
    ui.publish(false,true);
    assert(pads[0]->GetDeviceState(sizeof(state),&state)==DI_OK&&state.lX==32767);
    assert(state.rgbButtons[6]==0&&state.rgbButtons[7]==0&&state.rgbButtons[8]==0);
    publisher.page->pads[0].x=0;publisher.page->pads[0].buttons=0;publisher.commit();
    assert(pads[0]->GetDeviceState(sizeof(state),&state)==DI_OK);
    publisher.page->pads[0].x=32767;publisher.page->pads[0].buttons=1;publisher.commit();
    assert(pads[0]->GetDeviceState(sizeof(state),&state)==DI_OK&&state.lX==65535&&state.rgbButtons[0]==0x80);
    assert(pads[0]->SetCooperativeLevel(reinterpret_cast<HWND>(1),DISCL_FOREGROUND)==DI_OK);
    assert(pads[0]->GetDeviceState(sizeof(state),&state)==DIERR_INPUTLOST);
    // Explicit PID-local diagnostics alone may authorize a private-desktop pad.
    char diagnostic_name[96];std::snprintf(diagnostic_name,sizeof(diagnostic_name),"Local\\OpenSoccerMania-Input-%lu",GetCurrentProcessId());
    HANDLE diagnostic_mapping=CreateFileMappingA(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,272,diagnostic_name);assert(diagnostic_mapping);
    auto* diagnostic_page=static_cast<DWORD*>(MapViewOfFile(diagnostic_mapping,FILE_MAP_WRITE,0,0,272));assert(diagnostic_page);
    std::memset(diagnostic_page,0,272);diagnostic_page[0]=0x494d534f;diagnostic_page[1]=2;Sleep(110);
    for(DWORD flags:{DWORD(0),DWORD(1),DWORD(2)}){diagnostic_page[2]=flags;MemoryBarrier();assert(pads[0]->Acquire()==DIERR_INPUTLOST);}
    diagnostic_page[2]=3;diagnostic_page[0]=0;MemoryBarrier();assert(pads[0]->Acquire()==DIERR_INPUTLOST);
    diagnostic_page[0]=0x494d534f;diagnostic_page[1]=3;MemoryBarrier();assert(pads[0]->Acquire()==DIERR_INPUTLOST);
    diagnostic_page[1]=4;MemoryBarrier();publisher.commit();
    assert(pads[0]->Acquire()==DI_OK);
    assert(pads[0]->GetDeviceState(sizeof(state),&state)==DI_OK&&state.lX==65535&&state.rgbButtons[0]==0x80);
    for(unsigned i=1;i<4;++i){
        assert(pads[i]->SetCooperativeLevel(reinterpret_cast<HWND>(1),DISCL_FOREGROUND)==DI_OK&&pads[i]->Acquire()==DI_OK);
        DIJOYSTATE2 independent={};assert(pads[i]->GetDeviceState(sizeof(independent),&independent)==DI_OK);
        assert(independent.lX==(i==1?-1000:0)&&independent.lY==(i==2?-1000:i==3?1000:0));
        assert(pads[i]->SetCooperativeLevel(nullptr,DISCL_FOREGROUND)==DI_OK);
    }
    diagnostic_page[1]=5;MemoryBarrier();assert(pads[0]->GetDeviceState(sizeof(state),&state)==DIERR_INPUTLOST);
    diagnostic_page[1]=6;MemoryBarrier();assert(pads[0]->Acquire()==DI_OK);
    diagnostic_page[0]=0;MemoryBarrier();assert(pads[0]->GetDeviceState(sizeof(state),&state)==DIERR_INPUTLOST);
    diagnostic_page[0]=0x494d534f;diagnostic_page[1]=8;MemoryBarrier();assert(pads[0]->Acquire()==DI_OK);
    diagnostic_page[2]=1;diagnostic_page[1]=10;MemoryBarrier();
    assert(pads[0]->GetDeviceState(sizeof(state),&state)==DIERR_INPUTLOST&&state.lX==32767&&state.rgbButtons[0]==0);
    diagnostic_page[0]=0;UnmapViewOfFile(diagnostic_page);CloseHandle(diagnostic_mapping);
    assert(pads[0]->SetCooperativeLevel(nullptr,DISCL_FOREGROUND)==DI_OK&&pads[0]->Acquire()==DI_OK);
    DIPROPDWORD buffer={{sizeof(DIPROPDWORD),sizeof(DIPROPHEADER),0,DIPH_DEVICE},8};
    assert(pads[0]->Unacquire()==DI_OK);
    const HRESULT buffer_result=pads[0]->SetProperty(DIPROP_BUFFERSIZE,&buffer.diph);assert(buffer_result==DI_OK);
    assert(pads[0]->Acquire()==DI_OK);
    publisher.page->pads[0].buttons=0;publisher.commit();assert(pads[0]->Poll()==DI_OK);
    DIDEVICEOBJECTDATA events[8]={};DWORD count=8;
    assert(pads[0]->GetDeviceData(sizeof(events[0]),events,&count,DIGDD_PEEK)==DI_OK&&count==1&&events[0].dwOfs==DIJOFS_BUTTON(0)&&events[0].dwData==0);
    publisher.page->pads[0].buttons=2;publisher.commit();assert(pads[0]->Poll()==DI_OK);count=8;
    assert(pads[0]->GetDeviceData(sizeof(events[0]),events,&count,0)==DI_OK&&count==2&&events[1].dwOfs==DIJOFS_BUTTON(2)&&events[1].dwData==0x80);
    count=8;assert(pads[0]->GetDeviceData(sizeof(events[0]),events,&count,0)==DI_OK&&count==0);
    buffer.dwData=1;assert(pads[0]->SetProperty(DIPROP_BUFFERSIZE,&buffer.diph)==DI_OK);
    publisher.page->pads[0].buttons=15;publisher.commit();assert(pads[0]->Poll()==DI_OK);count=8;
    assert(pads[0]->GetDeviceData(sizeof(events[0]),events,&count,0)==DI_BUFFEROVERFLOW&&count==1);
    publisher.page->flags=1;publisher.commit();
    assert(pads[0]->GetDeviceState(sizeof(state),&state)==DI_OK);
    for(auto button:state.rgbButtons)assert(button==0); // held actions cannot change roles
    publisher.page->pads[0].buttons=0;publisher.page->pads[0].x=0;publisher.commit();assert(pads[0]->GetDeviceState(sizeof(state),&state)==DI_OK);
    publisher.page->pads[0].buttons=2;publisher.commit();assert(pads[0]->GetDeviceState(sizeof(state),&state)==DI_OK&&state.rgbButtons[1]==0x80&&state.rgbButtons[2]==0); // B backs out of menus
    publisher.page->flags=3;publisher.commit();assert(pads[0]->GetDeviceState(sizeof(state),&state)==DI_OK&&state.rgbButtons[1]==0&&state.rgbButtons[2]==0);
    publisher.page->pads[0].buttons=0;publisher.commit();assert(pads[0]->GetDeviceState(sizeof(state),&state)==DI_OK);
    publisher.page->pads[0].buttons=2;publisher.page->pads[0].x=32767;publisher.commit();assert(pads[0]->GetDeviceState(sizeof(state),&state)==DI_OK&&state.rgbButtons[1]==0&&state.rgbButtons[2]==0x80); // B shoots in a match
    DIDEVICEINSTANCEA info={};info.dwSize=sizeof(info);assert(pads[0]->GetDeviceInfo(&info)==DI_OK);
    assert(std::strcmp(info.tszInstanceName,"Gravis GamePad Pro (GamePort)")==0&&osm::pad_guid_slot(info.guidInstance)==0);
    DIDEVCAPS caps={};caps.dwSize=sizeof(caps);assert(pads[0]->GetCapabilities(&caps)==DI_OK&&caps.dwAxes==2&&caps.dwPOVs==0&&caps.dwButtons==12);
    publisher.page->pads[1].connected=0;publisher.commit();assert(pads[1]->GetDeviceState(sizeof(state),&state)==static_cast<HRESULT>(DIERR_UNPLUGGED));assert(state.lX==0&&state.lY==0&&state.rgbButtons[0]==0&&state.rgdwPOV[0]==0xffffffff);assert(pads[0]->GetDeviceState(sizeof(state),&state)==DI_OK);
    publisher.page->pads[1].connected=1;publisher.commit();assert(pads[1]->GetDeviceState(sizeof(state),&state)==DI_OK);
    // Real keyboard bytes survive, but pad mailbox keyboard emulation is rejected.
    char name[96];std::snprintf(name,sizeof(name),"Local\\OpenSoccerMania-Input-%lu",GetCurrentProcessId());
    HANDLE mapping=CreateFileMappingA(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,272,name);auto* input=static_cast<BYTE*>(MapViewOfFile(mapping,FILE_MAP_WRITE,0,0,272));std::memset(input,0,272);auto* words=reinterpret_cast<DWORD*>(input);words[0]=0x494d534f;words[1]=2;words[2]=5;input[16+DIK_UP]=0x80;
    original_state=physical_keyboard;BYTE keys[256]={};assert(device_state(nullptr,256,keys)==DI_OK);assert(keys[DIK_W]==0x80&&keys[DIK_UP]==0);
    words[2]=3;words[1]+=2;assert(device_state(nullptr,256,keys)==DI_OK);assert(keys[DIK_W]==0&&keys[DIK_UP]==0x80); // Private diagnostics replace physical input.
    // Skip commands may touch only Escape, only during a cinematic.
    game_window=GetForegroundWindow();words[2]=17;words[1]+=2;input[16+DIK_ESCAPE]=0x80;
    ui.publish(false,false,true);
    assert(device_state(nullptr,256,keys)==DI_OK&&keys[DIK_ESCAPE]==0x80&&keys[DIK_UP]==0&&keys[DIK_W]==0x80);
    ui.publish(false,false,false);
    assert(device_state(nullptr,256,keys)==DI_OK&&keys[DIK_ESCAPE]==0&&keys[DIK_UP]==0&&keys[DIK_W]==0x80);
    for(auto* pad:pads)assert(pad->Release()==0);
    UnmapViewOfFile(input);CloseHandle(mapping);
    if(argc==2) {
        // Exercise the actual proxy DLL against the system DirectInput factory,
        // without creating a window, launching the game, or injecting OS input.
        HMODULE proxy=LoadLibraryA(argv[1]);assert(proxy);
        const auto factory=reinterpret_cast<Factory>(GetProcAddress(proxy,"DirectInput8Create"));assert(factory);
        const GUID iid={0xbf798030,0x483a,0x4da2,{0xaa,0x99,0x5d,0x64,0xed,0x36,0x97,0}};
        IDirectInput8A* api=nullptr;assert(factory(GetModuleHandleA(nullptr),0x800,iid,reinterpret_cast<void**>(&api),nullptr)==DI_OK);
        ids.clear();assert(api->EnumDevices(DI8DEVCLASS_GAMECTRL,collect,&ids,DIEDFL_ATTACHEDONLY)==DI_OK);
        assert(ids.size()==4);
        for(unsigned i=0;i<4;++i) {
            assert(osm::pad_guid_slot(ids[i])==int(i));IDirectInputDevice8A* pad=nullptr;
            assert(api->CreateDevice(ids[i],&pad,nullptr)==DI_OK&&pad);
            assert(pad->SetDataFormat(&format)==DI_OK&&pad->Acquire()==DI_OK);
            DIJOYSTATE2 live={};assert(pad->GetDeviceState(sizeof(live),&live)==DI_OK);
            assert(live.lX==(i==0?1000:i==1?-1000:0));
            assert(live.rgdwPOV[0]==0xffffffff&&live.lVX==0&&live.lVY==0&&live.lAX==0&&live.lFY==0);
            const unsigned expected_button=i<2?2:i==2?1:3;
            for(unsigned button=0;button<128;++button)assert(live.rgbButtons[button]==(button==expected_button?0x80:0));
            DIDEVICEINSTANCEA device_info={};device_info.dwSize=sizeof(device_info);
            assert(pad->GetDeviceInfo(&device_info)==DI_OK&&std::strcmp(device_info.tszInstanceName,"Gravis GamePad Pro (GamePort)")==0);
            assert(pad->Release()==0);
        }
        api->Release(); // Proxy hooks, like the real game, live until process exit.
        std::puts("Real proxy DLL/system DirectInput factory: four separate native device GUIDs passed");
    }
    // A stopped producer cannot leave a player walking indefinitely.
    Sleep(600);osm::PadState stale={};assert(!pad_reader.read(0,stale));
    std::puts("Four native pads isolated; keyboard preserved; PS2 buttons and stale/disconnect guards passed");
}
