#pragma once
#include <windows.h>
#include <d3d8.h>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include "ui_state.h"
#include "performance.h"

namespace osm {
class SettingsMenu {
    UiStatePublisher publisher;
    bool open_=false,gameplay_=false,movie_=false,controller_=false,dirty_=true,changed_=true;
    bool keys_[256]={};ULONGLONG repeats_[256]={},refresh_=0;
    unsigned selected_=0;
    int zoom_=0,hud_=1,scale_=125,prompts_=1,limit_=240;
    char config_[MAX_PATH]={},log_[MAX_PATH]={},wrapper_[MAX_PATH]={};
    IDirect3DDevice8* texture_device_=nullptr;
    IDirect3DTexture8* texture_=nullptr;
    static int bounded(int value,int lo,int hi,int fallback){return value>=lo&&value<=hi?value:fallback;}
    void publish(){publisher.publish(open_,gameplay_,movie_);}
    void save() {
        const char* names[]={"CameraZoom","PerformanceHud","OverlayScale","XboxPrompts","RenderFPS"};
        const int values[]={zoom_,hud_,scale_,prompts_,limit_};
        bool success=true;
        for(unsigned i=0;i<5;++i){char value[16];std::snprintf(value,sizeof(value),"%d",values[i]);success=WritePrivateProfileStringA("Graphics",names[i],value,config_)&&success;}
        if(GetFileAttributesA(wrapper_)!=INVALID_FILE_ATTRIBUTES){char value[16];std::snprintf(value,sizeof(value),"%d",limit_);success=WritePrivateProfileStringA("GeneralExt","FPSLimit",value,wrapper_)&&success;}
        if(!success){FILE* file=std::fopen(log_,"a");if(file){std::fprintf(file,"SETTINGS failed to persist preferences error=%lu\n",GetLastError());std::fclose(file);}}
        dirty_=changed_=true;
    }
    void read() {
        const int z=bounded(GetPrivateProfileIntA("Graphics","CameraZoom",0,config_),0,2,0);
        const int h=GetPrivateProfileIntA("Graphics","PerformanceHud",1,config_)!=0;
        const int s=bounded(GetPrivateProfileIntA("Graphics","OverlayScale",125,config_),75,200,125);
        const int p=GetPrivateProfileIntA("Graphics","XboxPrompts",1,config_)!=0;
        int l=GetPrivateProfileIntA("Graphics","RenderFPS",240,config_);
        if(l!=0&&l!=60&&l!=120&&l!=144&&l!=165&&l!=240&&l!=360&&l!=480)l=240;
        if(z!=zoom_||h!=hud_||s!=scale_||p!=prompts_||l!=limit_)dirty_=changed_=true;
        zoom_=z;hud_=h;scale_=s;prompts_=p;limit_=l;
    }
    void change(int direction) {
        switch(selected_) {
        case 0:zoom_=(zoom_+direction+3)%3;break;
        case 1:hud_=!hud_;break;
        case 2:scale_=std::max(75,std::min(200,scale_+25*direction));break;
        case 3:prompts_=!prompts_;break;
        case 4:{constexpr int limits[]={60,120,144,165,240,360,480,0};int index=0;for(int i=0;i<8;++i)if(limits[i]==limit_)index=i;limit_=limits[(index+direction+8)%8];break;}
        }
        save();
    }
    bool paint(IDirect3DDevice8* device) {
        if(texture_device_!=device)release(texture_device_);
        if(!texture_) {
            if(FAILED(IDirect3DDevice8_CreateTexture(device,1024,1024,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&texture_)))return false;
            texture_device_=device;dirty_=true;
        }
        if(!dirty_)return true;
        HDC dc=CreateCompatibleDC(nullptr);if(!dc)return false;
        BITMAPINFO info={};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=1024;info.bmiHeader.biHeight=-1024;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        void* pixels=nullptr;HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
        if(!bitmap||!pixels){if(bitmap)DeleteObject(bitmap);DeleteDC(dc);return false;}
        HGDIOBJ old_bitmap=SelectObject(dc,bitmap);std::memset(pixels,0,1024*1024*4);
        auto box=[&](int x,int y,int w,int h,int radius,COLORREF color){HBRUSH brush=CreateSolidBrush(color);HGDIOBJ old=SelectObject(dc,brush),pen=SelectObject(dc,GetStockObject(NULL_PEN));RoundRect(dc,x,y,x+w,y+h,radius,radius);SelectObject(dc,old);SelectObject(dc,pen);DeleteObject(brush);};
        box(3,5,1018,758,40,RGB(12,35,28));box(0,0,1018,756,38,RGB(245,241,229));
        for(int y=24;y<730;y+=5)for(int x=24;x<994;x+=5)if(((x*17+y*11)%31)<5)SetPixelV(dc,x,y,RGB(237,233,220));
        SetBkMode(dc,TRANSPARENT);
        auto label=[&](int x,int y,const char* text,int size,bool bold,COLORREF color){HFONT font=CreateFontA(-size,0,0,0,bold?FW_BOLD:FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,"Nunito");HGDIOBJ old=SelectObject(dc,font);SetTextColor(dc,color);TextOutA(dc,x,y,text,static_cast<int>(std::strlen(text)));SelectObject(dc,old);DeleteObject(font);};
        label(40,27,"OpenSoccerMania",44,true,RGB(22,61,50));label(42,83,"GRAPHICS & CONTROLS",23,true,RGB(89,112,103));
        const char* labels[]={"Camera framing","Performance HUD","HUD size","Xbox buttons","Render limit"};
        const char* zooms[]={"Original zoom","Wide view (experimental)","Distant view (experimental)"};
        char scale[32],limit[48];std::snprintf(scale,sizeof(scale),"%d%%",scale_);std::snprintf(limit,sizeof(limit),limit_?"%d FPS (restart)":"Unlimited (restart)",limit_);
        const char* values[]={zooms[zoom_],hud_?"On":"Off",scale,prompts_?"On":"Off",limit};
        for(unsigned row=0;row<5;++row){const int y=137+static_cast<int>(row)*90;if(row==selected_)box(24,y-8,970,66,22,RGB(216,228,204));label(44,y+2,labels[row],30,true,RGB(22,61,50));label(445,y+3,values[row],27,false,RGB(45,76,62));}
        label(42,610,"Camera options apply immediately. Render limits need a restart.",23,false,RGB(89,112,103));
        label(42,645,"The match keeps running while this panel is open.",23,false,RGB(89,112,103));
        auto button=[&](int x,int y,const char* letter,COLORREF color){box(x,y,38,38,38,color);label(x+10,y+3,letter,25,true,RGB(255,255,255));};
        if(controller_&&prompts_){button(42,694,"A",RGB(42,130,73));label(92,699,"Change",23,true,RGB(22,61,50));button(245,694,"B",RGB(183,59,50));label(295,699,"Close",23,true,RGB(22,61,50));label(480,699,"D-pad: navigate   View + Start: close",22,true,RGB(22,61,50));}
        else label(42,699,"F1: close    Arrows: navigate    Enter: change    Esc: close",23,true,RGB(22,61,50));
        auto* rgba=static_cast<DWORD*>(pixels);for(unsigned i=0;i<1024*1024;++i)if(rgba[i]&0xffffff)rgba[i]|=0xff000000;
        D3DLOCKED_RECT locked={};const HRESULT status=IDirect3DTexture8_LockRect(texture_,0,&locked,nullptr,0);
        if(SUCCEEDED(status)){for(unsigned y=0;y<1024;++y)std::memcpy(static_cast<BYTE*>(locked.pBits)+y*locked.Pitch,rgba+y*1024,4096);IDirect3DTexture8_UnlockRect(texture_,0);}
        SelectObject(dc,old_bitmap);DeleteObject(bitmap);DeleteDC(dc);dirty_=FAILED(status);return SUCCEEDED(status);
    }
public:
    void configure(const char* directory,const char* config) {
        std::snprintf(config_,MAX_PATH,"%s",config);std::snprintf(log_,MAX_PATH,"%sopensoccermania-settings.log",directory);
        std::snprintf(wrapper_,MAX_PATH,"%sdgVoodoo.conf",directory);
        publisher.open();read();char font[MAX_PATH];GetPrivateProfileStringA("UI","FontPath","",font,sizeof(font),config_);if(*font)AddFontResourceExA(font,FR_PRIVATE,nullptr);
    }
    bool is_open()const{return open_;}
    bool hud_enabled()const{return hud_!=0;}
    bool prompts_enabled()const{return prompts_!=0;}
    int hud_scale()const{return scale_;}
    int zoom_index()const{return zoom_;}
    bool consume_changed(){const bool value=changed_;changed_=false;return value;}
    void set_controller(bool value){if(value!=controller_){controller_=value;dirty_=true;}}
    void close_panel(){if(open_){open_=false;publish();}}
    void sync_hud(bool enabled){if(enabled!=(hud_!=0)){hud_=enabled;save();}}
    void update(SHORT (*key)(int),bool active,bool gameplay,bool movie=false) {
        if(gameplay_!=gameplay)dirty_=true;
        gameplay_=gameplay;movie_=movie;const ULONGLONG now=GetTickCount64();
        if(now>=refresh_&&!open_){read();refresh_=now+1000;}
        auto pressed=[&](int code,bool repeat){const bool down=(key(code)&0x8000)!=0;const bool edge=down&&!keys_[code];keys_[code]=down;if(edge){repeats_[code]=now+300;return true;}if(down&&repeat&&now>=repeats_[code]){repeats_[code]=now+100;return true;}return false;};
        if(!active){open_=false;std::memset(keys_,0,sizeof(keys_));publish();return;}
        if(pressed(VK_F1,false)){open_=!open_;dirty_=true;}
        if(open_) {
            if(pressed(VK_ESCAPE,false))open_=false;
            if(pressed(VK_UP,true)){selected_=(selected_+4)%5;dirty_=true;}
            if(pressed(VK_DOWN,true)){selected_=(selected_+1)%5;dirty_=true;}
            if(pressed(VK_PRIOR,false)){selected_=0;dirty_=true;}
            if(pressed(VK_NEXT,false)){selected_=4;dirty_=true;}
            if(pressed(VK_LEFT,true))change(-1);
            if(pressed(VK_RIGHT,true)||pressed(VK_RETURN,false))change(1);
        }
        publish();
    }
    void release(IDirect3DDevice8* device){if(texture_&&texture_device_==device){auto* texture=texture_;texture_=nullptr;texture_device_=nullptr;dirty_=true;IDirect3DTexture8_Release(texture);}}
    void draw(IDirect3DDevice8* device,const RECT& output,float pixel_scale,OverlayDraw render) {
        if(!open_)return;
        if(!paint(device)){close_panel();return;}
        DWORD state=0;if(FAILED(IDirect3DDevice8_CreateStateBlock(device,D3DSBT_ALL,&state))){close_panel();return;}
        IDirect3DDevice8_SetVertexShader(device,D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1);IDirect3DDevice8_SetPixelShader(device,0);
        for(auto pair:{std::pair<D3DRENDERSTATETYPE,DWORD>{D3DRS_ZENABLE,FALSE},{D3DRS_ZWRITEENABLE,FALSE},{D3DRS_LIGHTING,FALSE},{D3DRS_FOGENABLE,FALSE},{D3DRS_CULLMODE,D3DCULL_NONE},{D3DRS_ALPHABLENDENABLE,TRUE},{D3DRS_SRCBLEND,D3DBLEND_SRCALPHA},{D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA},{D3DRS_ALPHATESTENABLE,FALSE},{D3DRS_STENCILENABLE,FALSE}})IDirect3DDevice8_SetRenderState(device,pair.first,pair.second);
        for(auto pair:{std::pair<D3DTEXTURESTAGESTATETYPE,DWORD>{D3DTSS_COLOROP,D3DTOP_MODULATE},{D3DTSS_COLORARG1,D3DTA_TEXTURE},{D3DTSS_COLORARG2,D3DTA_DIFFUSE},{D3DTSS_ALPHAOP,D3DTOP_MODULATE},{D3DTSS_ALPHAARG1,D3DTA_TEXTURE},{D3DTSS_ALPHAARG2,D3DTA_DIFFUSE},{D3DTSS_MINFILTER,D3DTEXF_LINEAR},{D3DTSS_MAGFILTER,D3DTEXF_LINEAR},{D3DTSS_ADDRESSU,D3DTADDRESS_CLAMP},{D3DTSS_ADDRESSV,D3DTADDRESS_CLAMP},{D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE}})IDirect3DDevice8_SetTextureStageState(device,0,pair.first,pair.second);
        IDirect3DDevice8_SetTextureStageState(device,1,D3DTSS_COLOROP,D3DTOP_DISABLE);IDirect3DDevice8_SetTexture(device,0,reinterpret_cast<IDirect3DBaseTexture8*>(texture_));
        const float width=std::min({660.f*pixel_scale,(output.right-output.left)*.92f,(output.bottom-output.top)*.88f/.75f}),height=width*.75f;
        const float x=(output.left+output.right-width)*.5f-.5f,y=(output.top+output.bottom-height)*.5f-.5f;
        const float top=0,bottom=.75f;
        OverlayVertex vertices[4]={{x,y,0,1,0xffffffff,0,top},{x+width,y,0,1,0xffffffff,1,top},{x,y+height,0,1,0xffffffff,0,bottom},{x+width,y+height,0,1,0xffffffff,1,bottom}};
        const HRESULT drawn=render(device,D3DPT_TRIANGLESTRIP,2,vertices,sizeof(OverlayVertex));
        IDirect3DDevice8_ApplyStateBlock(device,state);IDirect3DDevice8_DeleteStateBlock(device,state);
        if(FAILED(drawn))close_panel();
    }
};
}
