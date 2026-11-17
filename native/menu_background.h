#pragma once
#include <windows.h>
#include <d3d8.h>
#include <array>
#include <vector>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include "../third_party/minhook/include/MinHook.h"

namespace osm {
struct MenuClip {float left,top,right,bottom;};
struct MenuSpriteVertex {float x,y;DWORD color;float u,v;};
static_assert(sizeof(MenuSpriteVertex)==20,"Native 2D sprite stream layout");
struct MenuTile {void* sprite;float x,y;unsigned flags,color;std::array<MenuSpriteVertex,4> vertices={};float offset_x=0,offset_y=0;};
struct MenuSideTile {MenuTile tile;unsigned side;float shift;};
inline int menu_prompt_context(std::uintptr_t caller) {return caller==0x4f72d1?1:caller==0x4f7327?2:0;}
inline bool menu_background_signature(const unsigned char* image,std::size_t size,bool verified) {
    if(!verified||!image||size<0x12f4a0)return false;
    constexpr unsigned char sprite[]={0x81,0xec,0x84,0x00,0x00,0x00,0x8b,0x84,0x24,0x88,0x00,0x00,0x00};
    constexpr unsigned char backdrop[]={0x83,0xec,0x0c,0x53,0x55,0x56,0x8b,0x74,0x24,0x1c};
    constexpr unsigned char tile_call[]={0xe8,0x42,0x4d,0xfb,0xff};
    constexpr unsigned char back_call[]={0xe8,0xcf,0xce,0xfe,0xff};
    constexpr unsigned char select_call[]={0xe8,0x79,0xce,0xfe,0xff};
    constexpr unsigned char clip[]={0x6a,0x01,0xe8,0xa9,0x0c,0x00,0x00};
    return !std::memcmp(image+0xe41a0,sprite,sizeof(sprite))&&!std::memcmp(image+0x12f3d0,backdrop,sizeof(backdrop))&&
        !std::memcmp(image+0x12f459,tile_call,sizeof(tile_call))&&!std::memcmp(image+0xf72cc,back_call,sizeof(back_call))&&
        !std::memcmp(image+0xf7322,select_call,sizeof(select_call))&&!std::memcmp(image+0x15410,clip,sizeof(clip));
}
inline std::size_t menu_side_tiles(const MenuTile* tiles,std::size_t count,const MenuClip& clip,float scale,
                                    MenuSideTile* output,std::size_t capacity) {
    if(!tiles||!output||count!=80||!std::isfinite(scale)||scale<.15f||scale>=.9999f||
       clip.left!=0||clip.top!=0||clip.right!=640||clip.bottom!=480)return 0;
    bool cells[80]={};
    for(std::size_t i=0;i<count;++i) {
        const auto& t=tiles[i];
        if(!t.sprite||t.flags!=3||!std::isfinite(t.x)||!std::isfinite(t.y))return 0;
        const int x=static_cast<int>(std::lround(t.x/64)),y=static_cast<int>(std::lround(t.y/64));
        if(x<0||x>=10||y<0||y>=8||std::fabs(t.x-x*64.f)>.01f||std::fabs(t.y-y*64.f)>.01f||cells[y*10+x])return 0;
        cells[y*10+x]=true;
    }
    const float padding=320.f*(1.f/scale-1.f);
    const unsigned repeats=static_cast<unsigned>(std::ceil(padding/640.f));
    std::size_t needed=0;
    for(std::size_t i=0;i<count;++i)for(unsigned r=1;r<=repeats;++r)for(unsigned side=0;side<2;++side) {
        const float x=tiles[i].x+(side?1.f:-1.f)*r*640.f;
        if((!side&&x<0&&x+64>-padding)||(side&&x>=640&&x<640+padding)) {
            if(needed>=capacity)return 0;
            output[needed]={tiles[i],side,x-tiles[i].x};output[needed++].tile.x=x;
        }
    }
    return needed;
}
inline float menu_segment_distance(float x,float y,float ax,float ay,float bx,float by) {
    const float dx=bx-ax,dy=by-ay,t=std::clamp(((x-ax)*dx+(y-ay)*dy)/(dx*dx+dy*dy),0.f,1.f);
    return std::hypot(x-ax-t*dx,y-ay-t*dy);
}
inline std::vector<DWORD> menu_button_pixels(bool back) {
    std::vector<DWORD> pixels(64*64,0);
    const float a[][4]={{23,43,31.5f,22},{31.5f,22,40,43},{26,35,37,35}};
    const float b[][4]={{25,22,25,43},{25,22,35,22},{35,22,39,26},{39,26,38,29},{38,29,34,32},{25,32,34,32},{34,32,39,35},{39,35,39,39},{39,39,35,43},{35,43,25,43}};
    for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x) {
        const float distance=std::hypot(x-31.5f,y-31.5f),alpha=std::clamp(23.5f-distance,0.f,1.f);
        if(alpha==0)continue;
        float glyph=100;
        if(back){for(const auto& line:b)glyph=std::min(glyph,menu_segment_distance(x,y,line[0],line[1],line[2],line[3]));}
        else{for(const auto& line:a)glyph=std::min(glyph,menu_segment_distance(x,y,line[0],line[1],line[2],line[3]));}
        const float white=std::clamp(2.1f-glyph,0.f,1.f),shade=distance>20.8f?.55f:1.f+(31.5f-y)*.004f;
        const unsigned r=static_cast<unsigned>(std::clamp((back?205.f:45.f)*shade*(1-white)+245*white,0.f,255.f));
        const unsigned g=static_cast<unsigned>(std::clamp((back?55.f:155.f)*shade*(1-white)+245*white,0.f,255.f));
        const unsigned bl=static_cast<unsigned>(std::clamp((back?55.f:73.f)*shade*(1-white)+245*white,0.f,255.f));
        pixels[y*64+x]=(static_cast<DWORD>(std::lround(alpha*255))<<24)|(r<<16)|(g<<8)|bl;
    }
    return pixels;
}
inline thread_local IDirect3DDevice8* menu_prompt_device=nullptr;
inline thread_local IDirect3DTexture8* menu_prompt_texture=nullptr;
inline thread_local unsigned long long menu_prompt_swaps=0;
class MenuPromptTextureSwap {
    IDirect3DDevice8* device_=nullptr;IDirect3DBaseTexture8* original_=nullptr;bool changed_=false;
public:
    explicit MenuPromptTextureSwap(IDirect3DDevice8* device):device_(device) {
        if(!device||menu_prompt_device!=device||!menu_prompt_texture)return;
        if(FAILED(IDirect3DDevice8_GetTexture(device,0,&original_))||!original_)return;
        if(IDirect3DBaseTexture8_GetType(original_)!=D3DRTYPE_TEXTURE)return;
        D3DSURFACE_DESC desc={};
        if(FAILED(IDirect3DTexture8_GetLevelDesc(reinterpret_cast<IDirect3DTexture8*>(original_),0,&desc))||desc.Width!=64||desc.Height!=64)return;
        changed_=SUCCEEDED(IDirect3DDevice8_SetTexture(device,0,reinterpret_cast<IDirect3DBaseTexture8*>(menu_prompt_texture)));
        if(changed_)++menu_prompt_swaps;
    }
    ~MenuPromptTextureSwap(){if(changed_)IDirect3DDevice8_SetTexture(device_,0,original_);if(original_)IDirect3DBaseTexture8_Release(original_);}
    MenuPromptTextureSwap(const MenuPromptTextureSwap&)=delete;
};
using MenuGpuDraw=HRESULT (WINAPI *)(IDirect3DDevice8*,D3DPRIMITIVETYPE,UINT,const void*,UINT);
struct MenuBackgroundAdapter {bool (*backbuffer)()=nullptr;float (*hud_scale)()=nullptr;MenuGpuDraw draw_up=nullptr;};
struct MenuReplayResult {std::size_t planned=0;unsigned drawn=0;};
inline MenuReplayResult menu_draw_side_tiles(IDirect3DDevice8* device,MenuBackgroundAdapter adapter,const MenuTile* tiles,std::size_t tile_count,const MenuClip& clip) {
    if(!device||!adapter.backbuffer||!adapter.backbuffer()||!adapter.hud_scale||!adapter.draw_up)return {};
    std::array<MenuSideTile,1024> extra;
    const float scale=adapter.hud_scale();
    const auto count=menu_side_tiles(tiles,tile_count,clip,scale,extra.data(),extra.size());
    if(!count)return {};
    DWORD state=0;
    if(FAILED(IDirect3DDevice8_CreateStateBlock(device,D3DSBT_ALL,&state)))return {};
    struct Batch {IDirect3DBaseTexture8* texture;std::vector<MenuSpriteVertex> vertices;};
    std::vector<Batch> batches;
    for(std::size_t i=0;i<count;++i) {
        const auto& source=extra[i];auto vertices=source.tile.vertices;bool valid=true;
        for(unsigned j=0;j<4;++j) {
            const float x=source.tile.x-source.shift+(j&1?64.f:0.f),y=source.tile.y+(j&2?64.f:0.f);
            const auto& v=vertices[j];
            if(!std::isfinite(v.x)||!std::isfinite(v.y)||std::fabs(v.x-x)>.01f||std::fabs(v.y-y)>.01f||v.u!=(j&1?1.f:0.f)||v.v!=(j&2?1.f:0.f))valid=false;
        }
        if(!valid)continue;
        const auto* bitmap=static_cast<const unsigned char*>(source.tile.sprite);
        const auto* storage=*reinterpret_cast<void* const*>(bitmap+0x30);
        auto* texture=storage?*reinterpret_cast<IDirect3DBaseTexture8* const*>(storage):nullptr;
        if(!texture||IDirect3DBaseTexture8_GetType(texture)!=D3DRTYPE_TEXTURE)continue;
        auto batch=std::find_if(batches.begin(),batches.end(),[texture](const Batch& b){return b.texture==texture;});
        if(batch==batches.end()) {
            D3DSURFACE_DESC desc={};if(FAILED(IDirect3DTexture8_GetLevelDesc(reinterpret_cast<IDirect3DTexture8*>(texture),0,&desc))||desc.Width!=64||desc.Height!=64)continue;
            batches.push_back({texture,{}});batch=batches.end()-1;
        }
        for(auto& v:vertices){v.x+=source.shift+source.tile.offset_x;v.y+=source.tile.offset_y;}
        for(unsigned index:{0u,1u,2u,2u,1u,3u})batch->vertices.push_back(vertices[index]);
    }
    unsigned drawn=0;
    for(const auto& batch:batches)if(SUCCEEDED(IDirect3DDevice8_SetTexture(device,0,batch.texture))&&
        SUCCEEDED(adapter.draw_up(device,D3DPT_TRIANGLELIST,static_cast<UINT>(batch.vertices.size()/3),batch.vertices.data(),sizeof(MenuSpriteVertex))))drawn+=static_cast<unsigned>(batch.vertices.size()/6);
    IDirect3DDevice8_ApplyStateBlock(device,state);IDirect3DDevice8_DeleteStateBlock(device,state);
    return {count,drawn};
}
class MenuBackground {
    using Sprite=void (__cdecl *)(void*,float,float,unsigned,unsigned);
    using Backdrop=void (__cdecl *)(void*);
    using Flush=void (__cdecl *)();
    using GetClip=const MenuClip* (__cdecl *)();
    inline static MenuBackground* instance_=nullptr;
    Sprite sprite_=nullptr;Backdrop backdrop_=nullptr;Flush flush_=nullptr;GetClip get_clip_=nullptr;
    MenuBackgroundAdapter adapter_={};char config_[MAX_PATH]={},log_[MAX_PATH]={};
    IDirect3DDevice8* device_=nullptr;IDirect3DTexture8* buttons_[2]={};
    bool controller_=false,prompts_=true,capturing_=false,overflow_=false;ULONGLONG refresh_=0;
    std::array<MenuTile,512> tiles_={};std::size_t count_=0;
    bool ready()const{return device_&&adapter_.backbuffer&&adapter_.backbuffer();}
    bool texture(unsigned index) {
        if(buttons_[index])return true;
        if(!device_)return false;
        IDirect3DTexture8* value=nullptr;
        if(FAILED(IDirect3DDevice8_CreateTexture(device_,64,64,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&value)))return false;
        D3DLOCKED_RECT lock={};
        if(FAILED(IDirect3DTexture8_LockRect(value,0,&lock,nullptr,0))){IDirect3DTexture8_Release(value);return false;}
        const auto pixels=menu_button_pixels(index==0);
        for(unsigned y=0;y<64;++y)std::memcpy(static_cast<unsigned char*>(lock.pBits)+y*lock.Pitch,pixels.data()+y*64,64*4);
        const HRESULT result=IDirect3DTexture8_UnlockRect(value,0);
        if(FAILED(result)){IDirect3DTexture8_Release(value);return false;}
        buttons_[index]=value;return true;
    }
    static void __cdecl sprite_hook(void* image,float x,float y,unsigned flags,unsigned color) {
        auto& self=*instance_;const auto caller=reinterpret_cast<std::uintptr_t>(__builtin_return_address(0));
        if(self.capturing_&&caller==0x52f45e&&image&&*reinterpret_cast<const WORD*>(static_cast<unsigned char*>(image)+8)==64&&*reinterpret_cast<const WORD*>(static_cast<unsigned char*>(image)+10)==64) {
            self.sprite_(image,x,y,flags,color);
            if(self.count_<self.tiles_.size()) {
                auto& tile=self.tiles_[self.count_++];tile={image,x,y,flags,color};
                std::memcpy(tile.vertices.data(),reinterpret_cast<const void*>(0x66d5b0),sizeof(MenuSpriteVertex)*4);
                tile.offset_x=*reinterpret_cast<const float*>(0x5d0648);tile.offset_y=*reinterpret_cast<const float*>(0x5d064c);
            } else self.overflow_=true;
            return;
        }
        const int prompt=menu_prompt_context(caller);
        if(!prompt||!self.controller_||!self.ready()){self.sprite_(image,x,y,flags,color);return;}
        if(GetTickCount64()>=self.refresh_){self.prompts_=GetPrivateProfileIntA("Graphics","XboxPrompts",1,self.config_)!=0;self.refresh_=GetTickCount64()+1000;}
        if(!self.prompts_||!image||*reinterpret_cast<const WORD*>(static_cast<unsigned char*>(image)+8)!=64||*reinterpret_cast<const WORD*>(static_cast<unsigned char*>(image)+10)!=64||!self.texture(prompt-1)){self.sprite_(image,x,y,flags,color);return;}
        self.flush_();
        auto* saved_device=menu_prompt_device;auto* saved_texture=menu_prompt_texture;
        menu_prompt_device=self.device_;menu_prompt_texture=self.buttons_[prompt-1];
        const auto before=menu_prompt_swaps;self.sprite_(image,x,y,flags,color);self.flush_();
        menu_prompt_device=saved_device;menu_prompt_texture=saved_texture;
        static unsigned reports=0;
        if(reports<4){++reports;const auto* v=reinterpret_cast<const float*>(0x66d5b0);FILE* f=std::fopen(self.log_,"a");if(f){std::fprintf(f,"MENU_PROMPT icon=%c swaps=%llu sprite=64x64 uv=%.3f,%.3f:%.3f,%.3f\n",prompt==1?'B':'A',menu_prompt_swaps-before,v[3],v[4],v[18],v[19]);std::fclose(f);}}
    }
    static void __cdecl backdrop_hook(void* object) {
        auto& self=*instance_;
        if(self.capturing_||!self.ready()||!self.adapter_.hud_scale||!self.adapter_.draw_up){self.backdrop_(object);return;}
        const MenuClip original=*self.get_clip_();
        self.count_=0;self.overflow_=false;self.capturing_=true;self.backdrop_(object);self.capturing_=false;
        if(self.overflow_)return;
        self.flush_();
        const auto replay=menu_draw_side_tiles(self.device_,self.adapter_,self.tiles_.data(),self.count_,original);
        const float scale=self.adapter_.hud_scale();const auto drawn=replay.drawn;
        static unsigned reports=0;if(reports++<3){FILE* f=std::fopen(self.log_,"a");if(f){std::fprintf(f,"MENU_BACKGROUND native_tiles=%zu sidebar_tiles=%u hud_scale=%.6f clip_preserved=1 state_restored=1\n",self.count_,drawn,scale);std::fclose(f);}}
    }
public:
    void set_controller(bool value){controller_=value;}
    bool owns_device(IDirect3DDevice8* device)const{return device_==device;}
    void release(){device_=nullptr;for(auto*& texture:buttons_){auto* old=texture;texture=nullptr;if(old)IDirect3DTexture8_Release(old);}}
    void set_device(IDirect3DDevice8* device){if(device_!=device){release();device_=device;}}
    bool start(const char* config,const char* log,MenuBackgroundAdapter adapter) {
        std::snprintf(config_,sizeof(config_),"%s",config);std::snprintf(log_,sizeof(log_),"%s",log);adapter_=adapter;
        const auto* base=reinterpret_cast<const unsigned char*>(GetModuleHandleA(nullptr));
        if(reinterpret_cast<std::uintptr_t>(base)!=0x400000||!GetPrivateProfileIntA("Graphics","VerifiedExecutable",0,config_))return false;
        const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<0||dos->e_lfanew>0x1000-static_cast<LONG>(sizeof(IMAGE_NT_HEADERS32)))return false;
        const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS32*>(base+dos->e_lfanew);
        if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC||!menu_background_signature(base,nt->OptionalHeader.SizeOfImage,true))return false;
        instance_=this;flush_=reinterpret_cast<Flush>(0x415320);get_clip_=reinterpret_cast<GetClip>(0x415460);
        const auto a=MH_CreateHook(reinterpret_cast<void*>(0x4e41a0),reinterpret_cast<void*>(sprite_hook),reinterpret_cast<void**>(&sprite_));
        const auto b=MH_CreateHook(reinterpret_cast<void*>(0x52f3d0),reinterpret_cast<void*>(backdrop_hook),reinterpret_cast<void**>(&backdrop_));
        if(a!=MH_OK||b!=MH_OK){if(a==MH_OK)MH_RemoveHook(reinterpret_cast<void*>(0x4e41a0));if(b==MH_OK)MH_RemoveHook(reinterpret_cast<void*>(0x52f3d0));return false;}
        return true;
    }
};
inline MenuBackground menu_background;
}
