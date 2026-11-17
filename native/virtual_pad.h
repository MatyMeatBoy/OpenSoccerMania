#pragma once
#include <windows.h>
#include <dinput.h>
#include <vector>
#include <deque>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <new>
#include <cstdlib>
#include "controller_input.h"
#include "input_page.h"
#include "ui_state.h"
namespace osm {
inline void pad_info(unsigned slot,DIDEVICEINSTANCEA& info) {
    info={};info.dwSize=sizeof(info);info.guidInstance=pad_guid(slot);info.guidProduct=pad_guid(0);
    info.dwDevType=DI8DEVTYPE_GAMEPAD|(DI8DEVTYPEGAMEPAD_STANDARD<<8);
    // The original game selects its Gravis button profile by INSTANCE name.
    // Preserve our private per-slot GUIDs; only this compatibility name is fixed.
    std::snprintf(info.tszInstanceName,MAX_PATH,"Gravis GamePad Pro (GamePort)");
    std::snprintf(info.tszProductName,MAX_PATH,"OpenSoccerMania SDL Pad %u",slot+1);
    info.wUsagePage=1;info.wUsage=5;
}
inline GUID pad_object_guid(DWORD first){return {first,0xc9f3,0x11cf,{0xbf,0xc7,0x44,0x45,0x53,0x54,0,0}};}
inline DIDEVICEOBJECTINSTANCEA pad_object(unsigned index) {
    DIDEVICEOBJECTINSTANCEA object={};object.dwSize=sizeof(object);
    if(index<2){object.guidType=pad_object_guid(0xa36d02e0+index);object.dwOfs=index?DIJOFS_Y:DIJOFS_X;object.dwType=DIDFT_ABSAXIS|DIDFT_MAKEINSTANCE(index);object.dwFlags=DIDOI_ASPECTPOSITION;}
    else {object.guidType=pad_object_guid(0xa36d02f0);object.dwOfs=DIJOFS_BUTTON(index-2);object.dwType=DIDFT_PSHBUTTON|DIDFT_MAKEINSTANCE(index-2);}
    std::snprintf(object.tszName,MAX_PATH,index<2?(index?"Left stick Y":"Left stick X"):"PS2 action %u",index-1);
    return object;
}
class VirtualPad final:public IDirectInputDevice8A {
    struct Field {DWORD offset;unsigned kind,index;}; // axis, button, neutral POV
    volatile LONG references_=1;unsigned slot_;PadReader* reader_;UiStateReader* ui_;
    PrivateInputDiagnostics diagnostics_;
    HWND window_=nullptr;DWORD cooperative_=DISCL_FOREGROUND;bool acquired_=false;
    DWORD data_size_=0,buffer_size_=0,sequence_=0;LONG low_[8]={-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000},high_[8]={1000,1000,1000,1000,1000,1000,1000,1000};
    DWORD deadzone_=0,saturation_=10000;std::vector<Field> fields_;
    std::vector<unsigned char> previous_;std::deque<DIDEVICEOBJECTDATA> events_;
    DWORD blocked_buttons_=0;bool block_axes_=false,overflow_=false,have_context_=false,last_gameplay_=false;
    int property_axis(LPCDIPROPHEADER header)const {
        if(header->dwHow==DIPH_DEVICE)return -1;
        if(header->dwHow==DIPH_BYOFFSET&&header->dwObj<=DIJOFS_SLIDER(1)&&header->dwObj%4==0)return int(header->dwObj/4);
        if(header->dwHow==DIPH_BYID&&(DIDFT_GETTYPE(header->dwObj)&DIDFT_AXIS)&&DIDFT_GETINSTANCE(header->dwObj)<8)return int(DIDFT_GETINSTANCE(header->dwObj));
        return -2;
    }
    LONG axis(LONG value,unsigned index)const {
        if(index>=8)index=0;
        const LONG low=low_[index],high=high_[index];
        value=std::clamp(value,LONG(-32767),LONG(32767));
        const LONG center=low+(high-low)/2;
        if(std::abs(value)*10000LL<=32767LL*deadzone_)value=0;
        if(saturation_&&std::abs(value)*10000LL>=32767LL*saturation_)value=value<0?-32767:32767;
        return value<0?center+static_cast<LONG>((static_cast<long long>(center)-low)*value/32767):center+static_cast<LONG>((static_cast<long long>(high)-center)*value/32767);
    }
    void neutral(std::vector<unsigned char>& bytes)const {
        bytes.assign(data_size_,0);
        for(const auto& field:fields_)if(field.kind!=1) {
            const DWORD value=field.kind==2?0xffffffff:field.kind==3?0:static_cast<DWORD>(axis(0,field.index));
            std::memcpy(bytes.data()+field.offset,&value,4);
        }
    }
    HRESULT sample(std::vector<unsigned char>& bytes) {
        if(!acquired_)return DIERR_NOTACQUIRED;
        if((cooperative_&DISCL_FOREGROUND)&&window_&&GetForegroundWindow()!=window_&&!diagnostics_.enabled()){acquired_=false;return DIERR_INPUTLOST;}
        PadState state={};bool gameplay=false;if(!reader_->read(slot_,state,&gameplay))return DIERR_UNPLUGGED;
        if(have_context_&&last_gameplay_!=gameplay){blocked_buttons_|=state.buttons;block_axes_=true;}
        have_context_=true;last_gameplay_=gameplay;
        const bool panel=ui_&&ui_->panel_open();
        if(panel){blocked_buttons_|=state.buttons;block_axes_=true;}
        blocked_buttons_&=state.buttons;state.buttons&=~blocked_buttons_;
        if(block_axes_){if(!state.x&&!state.y&&!panel)block_axes_=false;state.x=state.y=0;}
        if(panel)state.buttons=0;
        bytes.assign(data_size_,0);
        // Verified Gravis profile: pass0, long-ball1, shoot2, power3,
        // switch/skill6, sprint7, pause8. PS2 positions become A/B/X/Y,
        // LB=sprint, RB=switch. Menu Button2/back must remain Xbox B.
        constexpr unsigned match_source[]={0,2,1,3,15,16,10,9,6,4,7,8};
        constexpr unsigned menu_source[]={0,1,2,3,15,16,10,9,6,4,7,8};
        const auto* source=gameplay?match_source:menu_source;
        for(const auto& field:fields_) {
            if(field.kind==0){const LONG value=field.index==0?axis(state.x,0):field.index==1?axis(state.y,1):axis(0,field.index);std::memcpy(bytes.data()+field.offset,&value,4);}
            else if(field.kind==1){bytes[field.offset]=field.index<12&&(state.buttons&(1u<<source[field.index]))?0x80:0;}
            else {const DWORD centered=field.kind==2?0xffffffff:0;std::memcpy(bytes.data()+field.offset,&centered,4);}
        }
        if(buffer_size_&&previous_.size()==bytes.size())for(const auto& field:fields_) {
            const unsigned size=field.kind==1?1:4;
            if(std::memcmp(previous_.data()+field.offset,bytes.data()+field.offset,size)) {
                DIDEVICEOBJECTDATA event={};event.dwOfs=field.offset;std::memcpy(&event.dwData,bytes.data()+field.offset,size);
                event.dwTimeStamp=GetTickCount();event.dwSequence=++sequence_;
                if(events_.size()>=buffer_size_){events_.pop_front();overflow_=true;}events_.push_back(event);
            }
        }
        previous_=bytes;return DI_OK;
    }
public:
    VirtualPad(unsigned slot,PadReader& reader,UiStateReader* ui=nullptr):slot_(slot),reader_(&reader),ui_(ui){}
    HRESULT WINAPI QueryInterface(REFIID id,void** output)noexcept override {
        if(!output)return E_POINTER;*output=nullptr;
        const GUID unknown={0,0,0,{0xc0,0,0,0,0,0,0,0x46}};
        const GUID devices[]={
            {0x54d41080,0xdc15,0x4833,{0xa4,0x1b,0x74,0x8f,0x73,0xa3,0x81,0x79}},
            {0x5944e680,0xc92e,0x11cf,{0xbf,0xc7,0x44,0x45,0x53,0x54,0,0}},
            {0x5944e682,0xc92e,0x11cf,{0xbf,0xc7,0x44,0x45,0x53,0x54,0,0}},
            {0x57d7c6bc,0x2356,0x11d3,{0x8e,0x9d,0,0xc0,0x4f,0x68,0x44,0xae}}};
        bool supported=!std::memcmp(&id,&unknown,sizeof(id));
        for(const auto& device:devices)supported|=!std::memcmp(&id,&device,sizeof(id));
        if(!supported)return E_NOINTERFACE;
        *output=static_cast<IDirectInputDevice8A*>(this);AddRef();return S_OK;
    }
    ULONG WINAPI AddRef()noexcept override{return static_cast<ULONG>(InterlockedIncrement(&references_));}
    ULONG WINAPI Release()noexcept override{const ULONG count=static_cast<ULONG>(InterlockedDecrement(&references_));if(!count)delete this;return count;}
    HRESULT WINAPI GetCapabilities(LPDIDEVCAPS caps)noexcept override {
        if(!caps||caps->dwSize!=sizeof(*caps))return DIERR_INVALIDPARAM;
        *caps={};caps->dwSize=sizeof(*caps);caps->dwFlags=DIDC_ATTACHED|DIDC_POLLEDDEVICE;
        caps->dwDevType=DI8DEVTYPE_GAMEPAD|(DI8DEVTYPEGAMEPAD_STANDARD<<8);caps->dwAxes=2;caps->dwButtons=12;return DI_OK;
    }
    HRESULT WINAPI EnumObjects(LPDIENUMDEVICEOBJECTSCALLBACKA callback,LPVOID context,DWORD flags)noexcept override {
        if(!callback)return DIERR_INVALIDPARAM;
        for(unsigned i=0;i<14;++i){const auto object=pad_object(i);if(flags&&(flags&DIDFT_GETTYPE(object.dwType))==0)continue;if(callback(&object,context)==DIENUM_STOP)break;}return DI_OK;
    }
    HRESULT WINAPI GetProperty(REFGUID property,LPDIPROPHEADER header)noexcept override {
        if(!header||header->dwHeaderSize!=sizeof(*header))return DIERR_INVALIDPARAM;
        // DirectInput properties are integer pseudo-GUID pointers (including
        // unaligned values 1/2/5), not actual GUID objects. Preserve the raw ABI
        // token through a volatile integer so optimized C++ cannot discard
        // valid DIPROP branches based on GUID-reference alignment assumptions.
        volatile std::uintptr_t raw_token=reinterpret_cast<std::uintptr_t>(&property);
        const auto id=raw_token;
        if(id==4&&header->dwSize==sizeof(DIPROPRANGE)){auto* range=reinterpret_cast<DIPROPRANGE*>(header);const int index=property_axis(header);if(index<-1)return DIERR_OBJECTNOTFOUND;range->lMin=low_[index<0?0:index];range->lMax=high_[index<0?0:index];return DI_OK;}
        if(header->dwSize!=sizeof(DIPROPDWORD))return DIERR_UNSUPPORTED;
        auto* data=reinterpret_cast<DIPROPDWORD*>(header);
        if(id==1)data->dwData=buffer_size_;else if(id==2)data->dwData=DIPROPAXISMODE_ABS;else if(id==3)data->dwData=1;else if(id==5)data->dwData=deadzone_;else if(id==6)data->dwData=saturation_;else if(id==15)data->dwData=slot_;else return DIERR_UNSUPPORTED;
        return DI_OK;
    }
    HRESULT WINAPI SetProperty(REFGUID property,LPCDIPROPHEADER header)noexcept override {
        if(!header||header->dwHeaderSize!=sizeof(*header))return DIERR_INVALIDPARAM;
        // DirectInput properties are integer pseudo-GUID pointers (including
        // unaligned values 1/2/5), not actual GUID objects. Preserve the raw ABI
        // token through a volatile integer so optimized C++ cannot discard
        // valid DIPROP branches based on GUID-reference alignment assumptions.
        volatile std::uintptr_t raw_token=reinterpret_cast<std::uintptr_t>(&property);
        const auto id=raw_token;
        if(id==4&&header->dwSize==sizeof(DIPROPRANGE)){const auto* range=reinterpret_cast<const DIPROPRANGE*>(header);if(range->lMin>=range->lMax||static_cast<long long>(range->lMax)-range->lMin>0x7fffffff)return DIERR_INVALIDPARAM;const int index=property_axis(header);if(index<-1)return DIERR_OBJECTNOTFOUND;for(unsigned i=0;i<8;++i)if(index<0||i==unsigned(index)){low_[i]=range->lMin;high_[i]=range->lMax;}return DI_OK;}
        if(header->dwSize!=sizeof(DIPROPDWORD))return DIERR_UNSUPPORTED;
        const DWORD value=reinterpret_cast<const DIPROPDWORD*>(header)->dwData;
        if(id==1){buffer_size_=std::min(DWORD(128),value);events_.clear();overflow_=false;}
        else if(id==2){if(value!=DIPROPAXISMODE_ABS)return DIERR_UNSUPPORTED;}
        else if(id==5){if(value>10000)return DIERR_INVALIDPARAM;deadzone_=value;}
        else if(id==6){if(value>10000)return DIERR_INVALIDPARAM;saturation_=value;}
        else return DIERR_UNSUPPORTED;
        return DI_OK;
    }
    HRESULT WINAPI Acquire()noexcept override {PadState state={};if(!reader_->read(slot_,state))return DIERR_UNPLUGGED;if(!data_size_)return DIERR_INVALIDPARAM;acquired_=true;std::vector<unsigned char> bytes;return sample(bytes);}
    HRESULT WINAPI Unacquire()noexcept override {acquired_=false;previous_.clear();events_.clear();overflow_=false;return DI_OK;}
    HRESULT WINAPI GetDeviceState(DWORD size,LPVOID data)noexcept override {if(!data||size!=data_size_)return DIERR_INVALIDPARAM;std::vector<unsigned char> bytes;const HRESULT result=sample(bytes);if(FAILED(result))neutral(bytes);std::memcpy(data,bytes.data(),bytes.size());return result;}
    HRESULT WINAPI GetDeviceData(DWORD size,LPDIDEVICEOBJECTDATA data,LPDWORD count,DWORD flags)noexcept override {
        if(!count||size!=sizeof(DIDEVICEOBJECTDATA)||(flags&~DIGDD_PEEK))return DIERR_INVALIDPARAM;
        if(!buffer_size_)return DIERR_NOTBUFFERED;
        std::vector<unsigned char> bytes;const HRESULT result=sample(bytes);if(FAILED(result)){*count=0;return result;}
        const DWORD available=std::min(*count,DWORD(events_.size()));
        if(data)for(DWORD i=0;i<available;++i)data[i]=events_[i];
        const HRESULT status=overflow_?DI_BUFFEROVERFLOW:DI_OK;
        if(!(flags&DIGDD_PEEK)){for(DWORD i=0;i<available;++i)events_.pop_front();overflow_=false;}*count=available;return status;
    }
    HRESULT WINAPI SetDataFormat(LPCDIDATAFORMAT format)noexcept override {
        if(acquired_)return DIERR_ACQUIRED;
        if(!format||format->dwSize!=sizeof(*format)||format->dwObjSize<sizeof(DIOBJECTDATAFORMAT)||!format->rgodf||!format->dwDataSize||format->dwDataSize>4096||format->dwNumObjs>256)return DIERR_INVALIDPARAM;
        std::vector<Field> fields;unsigned next_button=0,next_axis=0,next_pov=0;
        for(DWORD i=0;i<format->dwNumObjs;++i) {
            const auto& object=*reinterpret_cast<const DIOBJECTDATAFORMAT*>(reinterpret_cast<const BYTE*>(format->rgodf)+i*format->dwObjSize);
            const DWORD type=DIDFT_GETTYPE(object.dwType);unsigned kind=0,index=DIDFT_GETINSTANCE(object.dwType),bytes=4;
            if(type&DIDFT_BUTTON){kind=1;bytes=1;if(index==0xffff)index=next_button;next_button=index+1;}
            else if(type&DIDFT_POV){kind=2;if(index==0xffff)index=next_pov;next_pov=index+1;}
            else if(type&DIDFT_AXIS){if(index==0xffff)index=next_axis;next_axis=index+1;if(object.pguid){switch(object.pguid->Data1){case 0xa36d02e0:index=0;break;case 0xa36d02e1:index=1;break;case 0xa36d02e2:index=2;break;case 0xa36d02f4:index=3;break;case 0xa36d02f5:index=4;break;case 0xa36d02e3:index=5;break;case 0xa36d02e4:if(index<6||index>7)index=6;break;default:break;}}}
            else continue;
            if(kind==0&&(object.dwFlags&DIDOI_ASPECTMASK)&&((object.dwFlags&DIDOI_ASPECTMASK)!=DIDOI_ASPECTPOSITION))kind=3;
            if(object.dwOfs>format->dwDataSize||bytes>format->dwDataSize-object.dwOfs)return DIERR_INVALIDPARAM;
            fields.push_back({object.dwOfs,kind,index});
        }
        fields_=std::move(fields);data_size_=format->dwDataSize;previous_.clear();events_.clear();overflow_=false;return DI_OK;
    }
    HRESULT WINAPI SetEventNotification(HANDLE event)noexcept override {return event?DIERR_UNSUPPORTED:DI_OK;}
    HRESULT WINAPI SetCooperativeLevel(HWND window,DWORD flags)noexcept override {window_=window;cooperative_=flags;return DI_OK;}
    HRESULT WINAPI GetObjectInfo(LPDIDEVICEOBJECTINSTANCEA output,DWORD object,DWORD how)noexcept override {
        if(!output||output->dwSize!=sizeof(*output))return DIERR_INVALIDPARAM;
        for(unsigned i=0;i<14;++i){const auto info=pad_object(i);if((how==DIPH_BYOFFSET&&info.dwOfs==object)||(how==DIPH_BYID&&info.dwType==object)){*output=info;return DI_OK;}}return DIERR_OBJECTNOTFOUND;
    }
    HRESULT WINAPI GetDeviceInfo(LPDIDEVICEINSTANCEA output)noexcept override {if(!output||output->dwSize!=sizeof(*output))return DIERR_INVALIDPARAM;pad_info(slot_,*output);return DI_OK;}
    HRESULT WINAPI RunControlPanel(HWND,DWORD)noexcept override{return DIERR_UNSUPPORTED;}
    HRESULT WINAPI Initialize(HINSTANCE,DWORD,REFGUID)noexcept override{return DI_OK;}
    HRESULT WINAPI CreateEffect(REFGUID,LPCDIEFFECT,LPDIRECTINPUTEFFECT*,LPUNKNOWN)noexcept override{return DIERR_UNSUPPORTED;}
    HRESULT WINAPI EnumEffects(LPDIENUMEFFECTSCALLBACKA,LPVOID,DWORD)noexcept override{return DI_OK;}
    HRESULT WINAPI GetEffectInfo(LPDIEFFECTINFOA,REFGUID)noexcept override{return DIERR_UNSUPPORTED;}
    HRESULT WINAPI GetForceFeedbackState(LPDWORD)noexcept override{return DIERR_UNSUPPORTED;}
    HRESULT WINAPI SendForceFeedbackCommand(DWORD)noexcept override{return DIERR_UNSUPPORTED;}
    HRESULT WINAPI EnumCreatedEffectObjects(LPDIENUMCREATEDEFFECTOBJECTSCALLBACK,LPVOID,DWORD)noexcept override{return DI_OK;}
    HRESULT WINAPI Escape(LPDIEFFESCAPE)noexcept override{return DIERR_UNSUPPORTED;}
    HRESULT WINAPI Poll()noexcept override {std::vector<unsigned char> bytes;return sample(bytes);}
    HRESULT WINAPI SendDeviceData(DWORD,LPCDIDEVICEOBJECTDATA,LPDWORD,DWORD)noexcept override{return DIERR_UNSUPPORTED;}
    HRESULT WINAPI EnumEffectsInFile(LPCSTR,LPDIENUMEFFECTSINFILECALLBACK,LPVOID,DWORD)noexcept override{return DIERR_UNSUPPORTED;}
    HRESULT WINAPI WriteEffectToFile(LPCSTR,DWORD,LPDIFILEEFFECT,DWORD)noexcept override{return DIERR_UNSUPPORTED;}
    HRESULT WINAPI BuildActionMap(LPDIACTIONFORMATA,LPCSTR,DWORD)noexcept override{return DIERR_UNSUPPORTED;}
    HRESULT WINAPI SetActionMap(LPDIACTIONFORMATA,LPCSTR,DWORD)noexcept override{return DIERR_UNSUPPORTED;}
    HRESULT WINAPI GetImageInfo(LPDIDEVICEIMAGEINFOHEADERA)noexcept override{return DIERR_UNSUPPORTED;}
};
} // namespace osm
