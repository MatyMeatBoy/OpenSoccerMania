#pragma once
#include "frame_stats.h"
#include <windows.h>
#include <d3d8.h>
#include <cstdio>

namespace osm {
using OverlayDraw=HRESULT (WINAPI *)(IDirect3DDevice8*,D3DPRIMITIVETYPE,UINT,const void*,UINT);
struct OverlayVertex {float x,y,z,w;DWORD color;float u,v;};
class PerformanceHud {
    FrameStats stats;
    LARGE_INTEGER frequency={},last={};
    bool enabled=true,key_down=false;
    unsigned frames=0;
    IDirect3DTexture8* text_texture=nullptr;
    IDirect3DDevice8* text_device=nullptr;
    unsigned last_update=0;

    void text(IDirect3DDevice8* device) {
        if(text_device!=device){text_texture=nullptr;text_device=device;}
        if(!text_texture && FAILED(IDirect3DDevice8_CreateTexture(device,512,128,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&text_texture)))return;
        if(frames-last_update<20 && last_update)return;
        last_update=frames;
        const auto value=stats.summary();
        char message[512];std::snprintf(message,sizeof(message),
            "OpenSoccerMania | %.1f FPS\nFrame %.2f ms   Present %.2f ms\nP95 %.2f   P99 %.2f   Max %.2f ms\nGPU time: not measured     F10: hide",
            value.fps,value.average_ms,value.present_ms,value.p95_ms,value.p99_ms,value.max_ms);
        BITMAPINFO info={};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=512;info.bmiHeader.biHeight=-128;
        info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        void* bits=nullptr;HDC dc=CreateCompatibleDC(nullptr);HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);
        if(!bitmap || !bits){if(bitmap)DeleteObject(bitmap);DeleteDC(dc);return;}
        HGDIOBJ previous=SelectObject(dc,bitmap);
        RECT rect={0,0,512,128};FillRect(dc,&rect,static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        HFONT font=CreateFontA(-21,0,0,0,FW_MEDIUM,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,FF_MODERN,"Consolas");
        HGDIOBJ old_font=SelectObject(dc,font);SetTextColor(dc,RGB(220,255,225));SetBkColor(dc,RGB(0,0,0));
        DrawTextA(dc,message,-1,&rect,DT_LEFT|DT_TOP|DT_NOPREFIX);
        D3DLOCKED_RECT lock;
        if(SUCCEEDED(IDirect3DTexture8_LockRect(text_texture,0,&lock,nullptr,0))) {
            const DWORD* pixels=static_cast<const DWORD*>(bits);
            for(int y=0;y<128;++y){DWORD* dest=reinterpret_cast<DWORD*>(static_cast<unsigned char*>(lock.pBits)+y*lock.Pitch);
                for(int x=0;x<512;++x){DWORD c=pixels[y*512+x]&0xffffff;DWORD alpha=std::max({c&255,(c>>8)&255,(c>>16)&255});dest[x]=(alpha<<24)|(c?0x00ddffe3:0);}}
            IDirect3DTexture8_UnlockRect(text_texture,0);
        }
        SelectObject(dc,old_font);SelectObject(dc,previous);DeleteObject(font);DeleteObject(bitmap);DeleteDC(dc);
    }
public:
    void release(IDirect3DDevice8* device) {
        if(text_device!=device)return;
        auto* texture=text_texture;text_texture=nullptr;text_device=nullptr;last_update=0;
        if(texture)IDirect3DTexture8_Release(texture);
    }
    void configure(bool value){enabled=value;QueryPerformanceFrequency(&frequency);}
    void begin(HWND window) {
        LARGE_INTEGER now;QueryPerformanceCounter(&now);
        if(last.QuadPart && frequency.QuadPart)stats.record(1000.0*(now.QuadPart-last.QuadPart)/frequency.QuadPart,last_present);
        last=now;++frames;
        const bool down=window && GetForegroundWindow()==window && (GetAsyncKeyState(VK_F10)&0x8000);
        if(down&&!key_down)enabled=!enabled;key_down=down;
    }
    double last_present=0;
    FrameSummary summary()const{return stats.summary();}
    void draw(IDirect3DDevice8* device,OverlayDraw render) {
        if(!enabled || !render)return;
        text(device);if(!text_texture)return;
        DWORD state=0;if(FAILED(IDirect3DDevice8_CreateStateBlock(device,D3DSBT_ALL,&state)))return;
        D3DVIEWPORT8 viewport={};IDirect3DDevice8_GetViewport(device,&viewport);
        IDirect3DDevice8_SetVertexShader(device,D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1);
        IDirect3DDevice8_SetPixelShader(device,0);
        for(auto pair: {std::pair<D3DRENDERSTATETYPE,DWORD>{D3DRS_ZENABLE,FALSE},{D3DRS_ZWRITEENABLE,FALSE},{D3DRS_LIGHTING,FALSE},{D3DRS_FOGENABLE,FALSE},{D3DRS_CULLMODE,D3DCULL_NONE},{D3DRS_ALPHABLENDENABLE,TRUE},{D3DRS_SRCBLEND,D3DBLEND_SRCALPHA},{D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA},{D3DRS_ALPHATESTENABLE,FALSE},{D3DRS_STENCILENABLE,FALSE}})
            IDirect3DDevice8_SetRenderState(device,pair.first,pair.second);
        IDirect3DDevice8_SetTextureStageState(device,0,D3DTSS_COLOROP,D3DTOP_MODULATE);
        IDirect3DDevice8_SetTextureStageState(device,0,D3DTSS_COLORARG1,D3DTA_TEXTURE);
        IDirect3DDevice8_SetTextureStageState(device,0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);
        IDirect3DDevice8_SetTextureStageState(device,0,D3DTSS_ALPHAOP,D3DTOP_MODULATE);
        IDirect3DDevice8_SetTextureStageState(device,0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);
        IDirect3DDevice8_SetTextureStageState(device,0,D3DTSS_ALPHAARG2,D3DTA_DIFFUSE);
        IDirect3DDevice8_SetTextureStageState(device,0,D3DTSS_MINFILTER,D3DTEXF_LINEAR);
        IDirect3DDevice8_SetTextureStageState(device,0,D3DTSS_MAGFILTER,D3DTEXF_LINEAR);
        IDirect3DDevice8_SetTextureStageState(device,1,D3DTSS_COLOROP,D3DTOP_DISABLE);
        const float left=viewport.X+10.0f,top=viewport.Y+10.0f,width=280,height=113;
        auto rectangle=[&](float x,float y,float w,float h,DWORD color,bool textured) {
            IDirect3DDevice8_SetTexture(device,0,textured?reinterpret_cast<IDirect3DBaseTexture8*>(text_texture):nullptr);
            if(!textured)IDirect3DDevice8_SetTextureStageState(device,0,D3DTSS_COLOROP,D3DTOP_SELECTARG2);
            OverlayVertex vertices[4]={{x-.5f,y-.5f,0,1,color,0,0},{x+w-.5f,y-.5f,0,1,color,1,0},{x-.5f,y+h-.5f,0,1,color,0,1},{x+w-.5f,y+h-.5f,0,1,color,1,1}};
            render(device,D3DPT_TRIANGLESTRIP,2,vertices,sizeof(OverlayVertex));
        };
        IDirect3DDevice8_SetTextureStageState(device,0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG2);
        rectangle(left,top,width,height,0xd010211c,false);
        IDirect3DDevice8_SetTextureStageState(device,0,D3DTSS_COLOROP,D3DTOP_MODULATE);
        IDirect3DDevice8_SetTextureStageState(device,0,D3DTSS_ALPHAOP,D3DTOP_MODULATE);
        rectangle(left+7,top+5,266,67,0xffffffff,true);
        IDirect3DDevice8_SetTextureStageState(device,0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG2);
        auto history=stats.history();const float graph_top=top+80,graph_height=25;
        rectangle(left+7,graph_top,266,graph_height,0xff09140f,false);
        if(history.size()>1) {
            std::vector<OverlayVertex> line;line.reserve(history.size());
            for(size_t i=0;i<history.size();++i)line.push_back({left+7+266.0f*i/(history.size()-1),graph_top+graph_height-static_cast<float>(std::min(50.0,history[i]))*graph_height/50.0f,0,1,history[i]>33.4?0xffffb453:0xff7fe2a8,0,0});
            render(device,D3DPT_LINESTRIP,static_cast<UINT>(line.size()-1),line.data(),sizeof(OverlayVertex));
        }
        IDirect3DDevice8_ApplyStateBlock(device,state);IDirect3DDevice8_DeleteStateBlock(device,state);
    }
};
}
