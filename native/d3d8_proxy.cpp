// OpenSoccerMania compatibility bridge. Preserve the original simulation rate.
#define CINTERFACE
#define COBJMACROS
#include <windows.h>
#include <d3d8.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include "buffer_mirror.h"
#include "hook_chain.h"
#include "input_page.h"
#include "aspect.h"
#include "layout.h"
#include "screen_vertices.h"
#include "performance.h"
#include "settings_menu.h"
#include "../third_party/minhook/include/MinHook.h"
#include "movies.h"
#include "portable.h"
#include "engine_hooks.h"
#include "title_background.h"
namespace osm {void start_backend_probe(const char* directory);}
static osm::PerformanceHud performance_hud;
static osm::SettingsMenu settings_menu;
static HWND game_window=nullptr;

static HMODULE self_module;
static char directory[MAX_PATH], config_path[MAX_PATH], log_path[MAX_PATH];
static bool wide = false;
static float scale = 1.0f;
static float vertical_scale=1.0f;
static unsigned long presents=0, projections=0, up_draws=0, hud_draws=0, vb_draws=0;
static bool state_cache_enabled=true,shader_known=false,constant0_known=false,state_recording=false;
static IDirect3DDevice8* cached_state_device=nullptr;
static DWORD cached_shader=0;
static float cached_constant0[4]={};
static unsigned long long shader_queries=0,shader_hits=0,constant_queries=0,constant_hits=0,screen_locks=0;
static LONGLONG screen_lock_ticks=0;
static LARGE_INTEGER bridge_frequency={};
static unsigned profile_frames=0;
static void invalidate_state_cache(){shader_known=constant0_known=false;}
static void select_state_device(IDirect3DDevice8* device){if(cached_state_device!=device){cached_state_device=device;invalidate_state_cache();}}
static HRESULT current_shader(IDirect3DDevice8* device,DWORD* value) {
    select_state_device(device);
    if(state_cache_enabled&&!state_recording&&shader_known){*value=cached_shader;++shader_hits;return D3D_OK;}
    ++shader_queries;const HRESULT result=IDirect3DDevice8_GetVertexShader(device,value);
    if(SUCCEEDED(result)&&!state_recording){cached_shader=*value;shader_known=true;}return result;
}
static HRESULT current_constant0(IDirect3DDevice8* device,float* value) {
    select_state_device(device);
    if(state_cache_enabled&&!state_recording&&constant0_known){std::memcpy(value,cached_constant0,16);++constant_hits;return D3D_OK;}
    ++constant_queries;const HRESULT result=IDirect3DDevice8_GetVertexShaderConstant(device,0,value,1);
    if(SUCCEEDED(result)&&!state_recording){std::memcpy(cached_constant0,value,16);constant0_known=true;}return result;
}
static D3DMATRIX original_projection;
static bool have_projection=false;
static int display_mode=0; // 0 = windowed, 1 = borderless, 2 = exclusive fullscreen
static unsigned requested_width=1920,requested_height=1080;
static float raster_x=1.0f,raster_y=1.0f;
static float raster_offset_x=0,raster_offset_y=0;
static unsigned logical_width=640,logical_height=480;
static bool auto_window_aspect=true;
static RECT presentation_rect={0,0,1920,1080};
static bool on_backbuffer(IDirect3DDevice8* device);
using BackbufferFn=HRESULT (WINAPI *)(IDirect3DDevice8*,UINT,D3DBACKBUFFER_TYPE,IDirect3DSurface8**);
using SurfaceDescFn=HRESULT (WINAPI *)(IDirect3DSurface8*,D3DSURFACE_DESC*);
static BackbufferFn original_backbuffer;
static SurfaceDescFn original_surface_desc;
static IDirect3DSurface8* physical_backbuffer=nullptr;
static bool target_is_backbuffer=true;
using RenderTargetFn=HRESULT (WINAPI *)(IDirect3DDevice8*,IDirect3DSurface8*,IDirect3DSurface8*);
static RenderTargetFn original_render_target;
static HRESULT WINAPI render_target(IDirect3DDevice8* device,IDirect3DSurface8* target,IDirect3DSurface8* depth) {
    HRESULT status=original_render_target(device,target,depth);
    if(SUCCEEDED(status)&&target)target_is_backbuffer=target==physical_backbuffer;
    return status;
}
static HRESULT WINAPI surface_desc(IDirect3DSurface8* surface,D3DSURFACE_DESC* desc) {
    HRESULT status=original_surface_desc(surface,desc);
    if(SUCCEEDED(status)&&desc&&surface==physical_backbuffer&&!osm::title_background_drawing){desc->Width=logical_width;desc->Height=logical_height;}
    return status;
}
static HRESULT WINAPI backbuffer(IDirect3DDevice8* device,UINT index,D3DBACKBUFFER_TYPE type,IDirect3DSurface8** surface) {
    HRESULT status=original_backbuffer(device,index,type,surface);
    if(SUCCEEDED(status)&&surface&&*surface&&index==0) {
        physical_backbuffer=*surface;
        auto* table=const_cast<IDirect3DSurface8Vtbl*>((*surface)->lpVtbl);
        if(osm::hook_decision(table->GetDesc,surface_desc,original_surface_desc)==osm::HookDecision::Install) {
            DWORD protect;
            if(VirtualProtect(&table->GetDesc,sizeof(void*),PAGE_READWRITE,&protect)) {
                if(!original_surface_desc)original_surface_desc=table->GetDesc;table->GetDesc=surface_desc;
                VirtualProtect(&table->GetDesc,sizeof(void*),protect,&protect);
            }
        }
    }
    return status;
}
static float overlay_user_scale=1.25f;

using SetViewportFn=HRESULT (WINAPI *)(IDirect3DDevice8*,const D3DVIEWPORT8*);
using GetViewportFn=HRESULT (WINAPI *)(IDirect3DDevice8*,D3DVIEWPORT8*);
static SetViewportFn original_set_viewport;
static GetViewportFn original_get_viewport;
static HRESULT WINAPI set_viewport(IDirect3DDevice8* device,const D3DVIEWPORT8* viewport) {
    if(osm::title_background_drawing)return original_set_viewport(device,viewport);
    if(!viewport)return original_set_viewport(device,viewport);
    if(!on_backbuffer(device))return original_set_viewport(device,viewport);
    D3DVIEWPORT8 physical=*viewport;
    physical.X=static_cast<DWORD>(raster_offset_x+viewport->X*raster_x);physical.Y=static_cast<DWORD>(raster_offset_y+viewport->Y*raster_y);
    physical.Width=static_cast<DWORD>(viewport->Width*raster_x);physical.Height=static_cast<DWORD>(viewport->Height*raster_y);
    return original_set_viewport(device,&physical);
}
static HRESULT WINAPI get_viewport(IDirect3DDevice8* device,D3DVIEWPORT8* viewport) {
    HRESULT status=original_get_viewport(device,viewport);
    if(SUCCEEDED(status)&&viewport&&on_backbuffer(device)&&!osm::title_background_drawing){viewport->X=static_cast<DWORD>(std::max(0.f,(viewport->X-raster_offset_x)/raster_x)+.5f);viewport->Y=static_cast<DWORD>(std::max(0.f,(viewport->Y-raster_offset_y)/raster_y)+.5f);viewport->Width=static_cast<DWORD>(viewport->Width/raster_x+.5f);viewport->Height=static_cast<DWORD>(viewport->Height/raster_y+.5f);}
    return status;
}
static void render_parameters(D3DPRESENT_PARAMETERS& physical,const D3DPRESENT_PARAMETERS& logical) {
    const unsigned width=logical.BackBufferWidth?logical.BackBufferWidth:640;
    const unsigned height=logical.BackBufferHeight?logical.BackBufferHeight:480;
    logical_width=width;logical_height=height;
    physical.BackBufferWidth=requested_width;physical.BackBufferHeight=requested_height;
    raster_x=static_cast<float>(requested_width)/width;raster_y=static_cast<float>(requested_height)/height;
    if(!wide)raster_x=raster_y=std::min(raster_x,raster_y);
    raster_offset_x=(requested_width-width*raster_x)*.5f;raster_offset_y=(requested_height-height*raster_y)*.5f;
}
using ReleaseDeviceFn=ULONG (WINAPI *)(IDirect3DDevice8*);
static ReleaseDeviceFn original_release_device;
static ULONG WINAPI release_device(IDirect3DDevice8* device) {
    const bool title_device=osm::title_background.owns_device(device);
    if(title_device)osm::title_background.release();
    performance_hud.release(device);
    settings_menu.release(device);
    const ULONG references=original_release_device(device);
    if(title_device&&references)osm::title_background.set_device(device);
    return references;
}
static BOOL (WINAPI *original_clip)(const RECT*)=nullptr;
static BOOL (WINAPI *original_warp)(int,int)=nullptr;
static BOOL WINAPI clip_cursor(const RECT* area) {return original_clip(display_mode==2?area:nullptr);}
static BOOL WINAPI warp_cursor(int x,int y) {return display_mode==2?original_warp(x,y):TRUE;}
static bool global_hooks_ready=false;
static unsigned capture_frame=0;
static bool capture_sky_pending=false;
static unsigned capture_sky_layer=0;
static bool trace_screen_vertices=false;
static bool live_sprite_diagnostics=false;
static unsigned sprite_fix_mode=0;
static void capture_screen_layer(IDirect3DDevice8*,const void*,UINT,UINT,DWORD,bool,bool);
static void capture_backbuffer(IDirect3DDevice8* device,const char* tag="frame") {
    IDirect3DSurface8 *source=nullptr,*copy=nullptr;
    HRESULT status=IDirect3DDevice8_GetBackBuffer(device,0,D3DBACKBUFFER_TYPE_MONO,&source);
    D3DSURFACE_DESC desc={};if(SUCCEEDED(status))status=original_surface_desc?original_surface_desc(source,&desc):IDirect3DSurface8_GetDesc(source,&desc);
    if(SUCCEEDED(status))status=IDirect3DDevice8_CreateImageSurface(device,desc.Width,desc.Height,desc.Format,&copy);
    if(SUCCEEDED(status))status=IDirect3DDevice8_CopyRects(device,source,nullptr,0,copy,nullptr);
    D3DLOCKED_RECT locked;
    if(SUCCEEDED(status))status=IDirect3DSurface8_LockRect(copy,&locked,nullptr,D3DLOCK_READONLY);
    if(SUCCEEDED(status)) {
        if(desc.Format==D3DFMT_X8R8G8B8 || desc.Format==D3DFMT_A8R8G8B8) {
            char path[MAX_PATH];std::snprintf(path,MAX_PATH,"%sosm-%s-%lu.bmp",directory,tag,presents);
            FILE* file=std::fopen(path,"wb");
            if(file){BITMAPFILEHEADER header={};BITMAPINFOHEADER info={};
                header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(info);header.bfSize=header.bfOffBits+desc.Width*desc.Height*4;
                info.biSize=sizeof(info);info.biWidth=desc.Width;info.biHeight=-static_cast<LONG>(desc.Height);info.biPlanes=1;info.biBitCount=32;info.biCompression=BI_RGB;
                std::fwrite(&header,1,sizeof(header),file);std::fwrite(&info,1,sizeof(info),file);
                for(UINT y=0;y<desc.Height;++y)std::fwrite(static_cast<const char*>(locked.pBits)+y*locked.Pitch,4,desc.Width,file);
                std::fclose(file);
            }
        }
        IDirect3DSurface8_UnlockRect(copy);
    }
    if(copy)IDirect3DSurface8_Release(copy);if(source)IDirect3DSurface8_Release(source);
    FILE* file=std::fopen(log_path,"a");if(file){std::fprintf(file,"FRAME_CAPTURE result=0x%lx dimensions=%ux%u format=%u\n",static_cast<unsigned long>(status),desc.Width,desc.Height,desc.Format);std::fclose(file);}
}
static bool background_running=true;
static std::unordered_map<HWND,WNDPROC> window_downstreams;
static WNDPROC original_legacy_proc=nullptr;
static WINDOWPLACEMENT saved_window_placement={};
static bool have_window_placement=false;
static bool window_toggle_down=false;
static void toggle_borderless(HWND window);
static LRESULT CALLBACK legacy_proc(HWND window,UINT message,WPARAM wparam,LPARAM lparam) {
    // The backend owns the physical window. Keep the game's 640x480 coordinate model stable.
    if(display_mode!=2 && message==WM_SIZE)return 0;
    if(display_mode!=2&&background_running&&((message==WM_ACTIVATEAPP&&!wparam)||(message==WM_ACTIVATE&&LOWORD(wparam)==WA_INACTIVE)||message==WM_KILLFOCUS))return 0;
    return CallWindowProcA(original_legacy_proc,window,message,wparam,lparam);
}
static LRESULT CALLBACK game_proc(HWND window,UINT message,WPARAM wparam,LPARAM lparam) {
    if(message==WM_KILLFOCUS||(message==WM_ACTIVATEAPP&&!wparam))settings_menu.close_panel();
    if(settings_menu.is_open()&&((message>=WM_KEYFIRST&&message<=WM_KEYLAST)||(message>=WM_MOUSEFIRST&&message<=WM_MOUSELAST)))return 0;
    if((message==WM_KEYUP&&wparam==VK_F11)||(message==WM_SYSKEYUP&&wparam==VK_RETURN)) {
        window_toggle_down=false;
        return 0;
    }
    const bool toggle_key=(message==WM_KEYDOWN&&wparam==VK_F11)||
        (message==WM_SYSKEYDOWN&&wparam==VK_RETURN&&(lparam&(1L<<29)));
    if(display_mode!=2&&toggle_key) {
        if(!(lparam&(1L<<30))&&!window_toggle_down)toggle_borderless(window);
        window_toggle_down=true;
        return 0;
    }
    if(display_mode==0 && message==WM_STYLECHANGING && wparam==static_cast<WPARAM>(GWL_STYLE)) {
        auto* style=reinterpret_cast<STYLESTRUCT*>(lparam);
        style->styleNew=(style->styleNew&~WS_POPUP)|WS_OVERLAPPEDWINDOW;
        return 0;
    }
    if(display_mode==0 && message==WM_GETMINMAXINFO) {
        auto* bounds=reinterpret_cast<MINMAXINFO*>(lparam);
        bounds->ptMinTrackSize={480,320};
        return DefWindowProcA(window,message,wparam,lparam);
    }
    if(display_mode!=2 && background_running &&
       ((message==WM_ACTIVATEAPP && !wparam) || (message==WM_ACTIVATE && LOWORD(wparam)==WA_INACTIVE) || message==WM_KILLFOCUS)) {
        ClipCursor(nullptr);ReleaseCapture();
        FILE* file=std::fopen(log_path,"a");if(file){std::fprintf(file,"BACKGROUND continue message=0x%x input_capture=released\n",message);std::fclose(file);}
        return 0;
    }
    const auto found=window_downstreams.find(window);
    const WNDPROC downstream=found==window_downstreams.end()?nullptr:found->second;
    const auto result=downstream?CallWindowProcA(downstream,window,message,wparam,lparam):DefWindowProcA(window,message,wparam,lparam);
    if(message==WM_NCDESTROY){window_downstreams.erase(window);if(game_window==window)game_window=nullptr;}
    return result;
}
static SHORT (WINAPI *original_async_key)(int)=nullptr;
static osm::PrivateKeyboard private_keyboard;
static SHORT WINAPI async_key(int key) {
    if(settings_menu.is_open())return 0;
    const bool focused=game_window&&GetForegroundWindow()==game_window;
    const SHORT injected=private_keyboard.state(key,focused);
    if(injected)return injected;
    if(game_window && !focused)return 0;
    return original_async_key(key);
}
static SHORT settings_key(int key) {
    const bool focused=game_window&&GetForegroundWindow()==game_window;
    const SHORT injected=private_keyboard.state(key,focused);
    return injected?injected:focused&&original_async_key?original_async_key(key):0;
}
static int (WINAPI *original_message_box)(HWND,LPCSTR,LPCSTR,UINT)=nullptr;
static int WINAPI message_box(HWND window,LPCSTR text,LPCSTR title,UINT flags) {
    FILE* file=std::fopen(log_path,"a");
    if(file){std::fprintf(file,"MESSAGE_BOX title=%s flags=0x%x text=%s\n",title?title:"",flags,text?text:"");std::fclose(file);}
    return original_message_box(window,text,title,flags);
}
static LONG CALLBACK exception_log(EXCEPTION_POINTERS* exception) {
    const DWORD code=exception->ExceptionRecord->ExceptionCode;
    if(code==EXCEPTION_ACCESS_VIOLATION || code==EXCEPTION_INT_DIVIDE_BY_ZERO || code==EXCEPTION_STACK_OVERFLOW) {
        FILE* file=std::fopen(log_path,"a");
        if(file){std::fprintf(file,"FIRST_CHANCE_EXCEPTION code=0x%lx address=%p (may be handled by application)\n",code,exception->ExceptionRecord->ExceptionAddress);std::fclose(file);}
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
using CreateShaderFn=HRESULT (WINAPI *)(IDirect3DDevice8*,const DWORD*,const DWORD*,DWORD*,DWORD);
static CreateShaderFn real_create_shader;
static std::unordered_set<DWORD> clip_shaders;
static std::unordered_set<DWORD> projection_shaders;
static std::unordered_set<DWORD> programmable_shaders;
static bool screen_format(DWORD value){return osm::screen_fvf(value)&&!programmable_shaders.count(value);}
using DeleteShaderFn=HRESULT (WINAPI *)(IDirect3DDevice8*,DWORD);
static DeleteShaderFn real_delete_shader;
static HRESULT WINAPI delete_shader(IDirect3DDevice8* device,DWORD handle) {
    const HRESULT result=real_delete_shader(device,handle);
    if(SUCCEEDED(result)){programmable_shaders.erase(handle);clip_shaders.erase(handle);projection_shaders.erase(handle);invalidate_state_cache();}
    return result;
}
static HRESULT WINAPI create_shader(IDirect3DDevice8* device,const DWORD* declaration,const DWORD* function,DWORD* handle,DWORD usage) {
    std::vector<DWORD> corrected;
    bool patched=false;
    if(wide&&!osm::camera_hooks_active&&function) {
        unsigned length=0;while(length<4096&&function[length]!=0xffff)++length;
        if(length<4096)for(unsigned i=0;i<=length;++i) {
            if(i+2<length&&function[i]==1&&function[i+1]==0xc00f0000&&function[i+2]==0x80e40008) {
                corrected.insert(corrected.end(),{5,0xc00f0000,0x80e40008,0xa0e4005f});i+=2;patched=true;
            } else corrected.push_back(function[i]);
        }
    }
    HRESULT result=real_create_shader(device,declaration,patched?corrected.data():function,handle,usage);
    if(FAILED(result)&&patched){patched=false;result=real_create_shader(device,declaration,function,handle,usage);}
    if(SUCCEEDED(result)&&handle)programmable_shaders.insert(*handle);
    if(SUCCEEDED(result)&&patched&&handle)projection_shaders.insert(*handle);
    if(SUCCEEDED(result) && function && handle) {
        SIZE_T words=0;while(words<4096 && function[words]!=0x0000ffff)++words;
        if(words<4096) {
            using Disassemble=HRESULT (WINAPI *)(const void*,SIZE_T,UINT,const char*,void**);
            HMODULE compiler=LoadLibraryA("d3dcompiler_47.dll");
            auto disassemble=compiler?reinterpret_cast<Disassemble>(GetProcAddress(compiler,"D3DDisassemble")):nullptr;
            void* blob=nullptr;
            if(disassemble && SUCCEEDED(disassemble(function,(words+1)*4,0,nullptr,&blob)) && blob) {
                auto table=*reinterpret_cast<void***>(blob);
                auto pointer=reinterpret_cast<void* (WINAPI *)(void*)>(table[3]);
                auto length=reinterpret_cast<SIZE_T (WINAPI *)(void*)>(table[4]);
                std::string assembly(static_cast<const char*>(pointer(blob)),length(blob));
                if(assembly.find("mul oPos.x, r0.x, c0.x")!=std::string::npos &&
                   (!osm::camera_hooks_active||assembly.find("mov oPos.w, v0.w")==std::string::npos))clip_shaders.insert(*handle);
                char filename[MAX_PATH];std::snprintf(filename,MAX_PATH,"%sosm-shader-%lu.txt",directory,*handle);
                FILE* file=std::fopen(filename,"wb");if(file){std::fwrite(pointer(blob),1,length(blob),file);std::fclose(file);}
                reinterpret_cast<ULONG (WINAPI *)(void*)>(table[2])(blob);
            }
            if(compiler)FreeLibrary(compiler);
        }
    }
    return result;
}

using CreateDeviceFn = HRESULT (WINAPI *)(IDirect3D8*,UINT,D3DDEVTYPE,HWND,DWORD,D3DPRESENT_PARAMETERS*,IDirect3DDevice8**);
using TransformFn = HRESULT (WINAPI *)(IDirect3DDevice8*,D3DTRANSFORMSTATETYPE,const D3DMATRIX*);
using GetTransformFn = HRESULT (WINAPI *)(IDirect3DDevice8*,D3DTRANSFORMSTATETYPE,D3DMATRIX*);
using PresentFn = HRESULT (WINAPI *)(IDirect3DDevice8*,const RECT*,const RECT*,HWND,const RGNDATA*);
using DrawUPFn = HRESULT (WINAPI *)(IDirect3DDevice8*,D3DPRIMITIVETYPE,UINT,const void*,UINT);
using DrawIndexedUPFn = HRESULT (WINAPI *)(IDirect3DDevice8*,D3DPRIMITIVETYPE,UINT,UINT,UINT,const void*,D3DFORMAT,const void*,UINT);
using DrawFn = HRESULT (WINAPI *)(IDirect3DDevice8*,D3DPRIMITIVETYPE,UINT,UINT);
using DrawIndexedFn = HRESULT (WINAPI *)(IDirect3DDevice8*,D3DPRIMITIVETYPE,UINT,UINT,UINT,UINT);
static CreateDeviceFn real_create;
static TransformFn real_transform;
static GetTransformFn real_get_transform;
static PresentFn real_present;
static DrawUPFn real_draw_up;
static DrawIndexedUPFn real_draw_indexed_up;
static HRESULT WINAPI overlay_draw(IDirect3DDevice8* device,D3DPRIMITIVETYPE type,UINT count,const void* data,UINT stride) {
    if(!data || stride!=sizeof(osm::OverlayVertex))return real_draw_up(device,type,count,data,stride);
    RECT client={};GetClientRect(game_window,&client);
    const float width=static_cast<float>(std::max(1L,client.right-client.left));
    const float height=static_cast<float>(std::max(1L,client.bottom-client.top));
    const float output_width=static_cast<float>(presentation_rect.right-presentation_rect.left);
    const float output_height=static_cast<float>(presentation_rect.bottom-presentation_rect.top);
    const float factor=std::max(output_width/width,output_height/height)*overlay_user_scale;
    UINT vertices=type==D3DPT_TRIANGLESTRIP?count+2:type==D3DPT_LINESTRIP?count+1:0;
    if(!vertices)return real_draw_up(device,type,count,data,stride);
    const auto* original=static_cast<const osm::OverlayVertex*>(data);
    std::vector<osm::OverlayVertex> scaled(original,original+vertices);
    for(auto& vertex:scaled){vertex.x=presentation_rect.left+(vertex.x+.5f)*factor-.5f;vertex.y=presentation_rect.top+(vertex.y+.5f)*factor-.5f;}
    return real_draw_up(device,type,count,scaled.data(),stride);
}

static DrawFn real_draw;
static DrawIndexedFn real_draw_indexed;
using ShaderConstantFn=HRESULT (WINAPI *)(IDirect3DDevice8*,DWORD,const void*,DWORD);
static ShaderConstantFn real_shader_constant;
using ShaderSetterFn=HRESULT (WINAPI *)(IDirect3DDevice8*,DWORD);
using BeginStateFn=HRESULT (WINAPI *)(IDirect3DDevice8*);
using EndStateFn=HRESULT (WINAPI *)(IDirect3DDevice8*,DWORD*);
static ShaderSetterFn real_set_shader;
static BeginStateFn real_begin_state;
static EndStateFn real_end_state;
static HRESULT WINAPI set_shader(IDirect3DDevice8* device,DWORD value) {
    const HRESULT result=real_set_shader(device,value);
    if(SUCCEEDED(result)){select_state_device(device);if(state_recording)shader_known=false;else{cached_shader=value;shader_known=true;}}return result;
}
static HRESULT WINAPI begin_state(IDirect3DDevice8* device) {
    const HRESULT result=real_begin_state(device);if(SUCCEEDED(result)){state_recording=true;invalidate_state_cache();}return result;
}
static HRESULT WINAPI end_state(IDirect3DDevice8* device,DWORD* token) {
    const HRESULT result=real_end_state(device,token);if(SUCCEEDED(result)){state_recording=false;invalidate_state_cache();}return result;
}
static bool correction_dirty=true;
static float applied_x=0,applied_y=0;
using ApplyStateFn=HRESULT (WINAPI *)(IDirect3DDevice8*,DWORD);
static ApplyStateFn original_apply_state;
static void apply_projection_correction(IDirect3DDevice8* device) {
    if(!wide||projection_shaders.empty()||(!correction_dirty&&applied_x==scale&&applied_y==vertical_scale))return;
    const float correction[4]={scale,vertical_scale,1,1};
    if(SUCCEEDED(real_shader_constant(device,95,correction,1))){applied_x=scale;applied_y=vertical_scale;correction_dirty=false;}
}
static HRESULT WINAPI apply_state(IDirect3DDevice8* device,DWORD token) {
    HRESULT result=original_apply_state(device,token);correction_dirty=true;
    if(SUCCEEDED(result)){invalidate_state_cache();apply_projection_correction(device);}return result;
}
using ResetFn=HRESULT (WINAPI *)(IDirect3DDevice8*,D3DPRESENT_PARAMETERS*);
static ResetFn real_reset;

static void window_parameters(D3DPRESENT_PARAMETERS* params) {
    if(display_mode==2 || !params)return;
    params->Windowed=TRUE;
    params->FullScreen_RefreshRateInHz=0;
    params->FullScreen_PresentationInterval=D3DPRESENT_INTERVAL_DEFAULT;
}

static void install_window_hooks(HWND window) {
    if(display_mode==2||!window||window_downstreams.count(window))return;
    const auto procedure=reinterpret_cast<void*>(GetClassLongPtrA(window,GCLP_WNDPROC));
    const auto base=reinterpret_cast<unsigned char*>(GetModuleHandleA(nullptr));
    const auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    const auto headers=reinterpret_cast<IMAGE_NT_HEADERS*>(base+dos->e_lfanew);
    const auto address=reinterpret_cast<ULONG_PTR>(procedure),begin=reinterpret_cast<ULONG_PTR>(base);
    if(!original_legacy_proc&&address>=begin&&address<begin+headers->OptionalHeader.SizeOfImage) {
        const auto created=MH_CreateHook(procedure,reinterpret_cast<void*>(legacy_proc),reinterpret_cast<void**>(&original_legacy_proc));
        if(created==MH_OK)MH_EnableHook(procedure);
    }
    // Install below the backend before its first CreateDevice. Later backend
    // wrappers may retain this callback; never capture them as our downstream.
    const auto current=reinterpret_cast<WNDPROC>(GetWindowLongPtrA(window,GWLP_WNDPROC));
    if(!current||current==game_proc)return;
    window_downstreams.emplace(window,current);
    SetLastError(0);
    const auto previous=SetWindowLongPtrA(window,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(game_proc));
    if(!previous&&GetLastError()){window_downstreams.erase(window);return;}
    window_downstreams[window]=reinterpret_cast<WNDPROC>(previous);
    FILE* file=std::fopen(log_path,"a");if(file){std::fprintf(file,"WINDOW class_proc=%p resize_owner=backend legacy_hook=%d downstream=%p\n",procedure,original_legacy_proc!=nullptr,reinterpret_cast<void*>(previous));std::fclose(file);}
}
static void window_appearance(HWND window) {
    if(display_mode==2 || !window)return;
    MONITORINFO monitor={};monitor.cbSize=sizeof(monitor);
    if(!GetMonitorInfoA(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor))return;
    const LONG style=display_mode==1 ? WS_POPUP|WS_VISIBLE : WS_OVERLAPPEDWINDOW|WS_VISIBLE;
    SetWindowLongA(window,GWL_STYLE,style);
    LONG extended=GetWindowLongA(window,GWL_EXSTYLE)&~WS_EX_TOPMOST;
    SetWindowLongA(window,GWL_EXSTYLE,extended);
    RECT rect=monitor.rcMonitor;
    if(display_mode==0) {
        const RECT area=monitor.rcWork;
        int width=960,height=static_cast<int>(960.0*requested_height/requested_width);
        if(width>area.right-area.left-80)width=area.right-area.left-80;
        if(height>area.bottom-area.top-100)height=area.bottom-area.top-100;
        rect={0,0,width,height};AdjustWindowRectEx(&rect,style,FALSE,extended);
        width=rect.right-rect.left;height=rect.bottom-rect.top;
        rect.left=area.left+(area.right-area.left-width)/2;
        rect.top=area.top+(area.bottom-area.top-height)/2;
        rect.right=rect.left+width;rect.bottom=rect.top+height;
    }
    SetWindowPos(window,HWND_NOTOPMOST,rect.left,rect.top,rect.right-rect.left,rect.bottom-rect.top,SWP_FRAMECHANGED|SWP_NOACTIVATE);
    ClipCursor(nullptr);ReleaseCapture();
}

static void toggle_borderless(HWND window) {
    if(!window||display_mode==2)return;
    if(display_mode==0) {
        saved_window_placement.length=sizeof(saved_window_placement);
        have_window_placement=GetWindowPlacement(window,&saved_window_placement)!=FALSE;
        display_mode=1;
        ShowWindow(window,SW_SHOWNOACTIVATE);
        window_appearance(window);
    } else {
        display_mode=0;
        window_appearance(window);
        if(have_window_placement) {
            auto placement=saved_window_placement;
            if(GetForegroundWindow()!=window)placement.showCmd=SW_SHOWNOACTIVATE;
            SetWindowPlacement(window,&placement);
        }
    }
    FILE* file=std::fopen(log_path,"a");
    if(file){RECT rect={};GetWindowRect(window,&rect);std::fprintf(file,"WINDOW_MODE source=hotkey mode=%s rect=%ld,%ld,%ld,%ld internal=%ux%u\n",display_mode==1?"Borderless":"Windowed",rect.left,rect.top,rect.right,rect.bottom,requested_width,requested_height);std::fclose(file);}
}

static HRESULT WINAPI reset(IDirect3DDevice8* device,D3DPRESENT_PARAMETERS* params) {
    osm::title_background.release();osm::reset_title_frame();
    invalidate_state_cache();
    performance_hud.release(device);
    settings_menu.release(device);
    physical_backbuffer=nullptr;
    if(!params)return D3DERR_INVALIDCALL;
    install_window_hooks(params->hDeviceWindow?params->hDeviceWindow:game_window);
    D3DPRESENT_PARAMETERS physical=*params;window_parameters(&physical);render_parameters(physical,*params);
    const HRESULT result=real_reset(device,&physical);
    FILE* file=std::fopen(log_path,"a");if(file){std::fprintf(file,"RESET result=0x%lx size=%ux%u windowed=%d\n",static_cast<unsigned long>(result),physical.BackBufferWidth,physical.BackBufferHeight,physical.Windowed);std::fclose(file);}
    if(SUCCEEDED(result)){osm::title_background.set_device(device);window_appearance(params->hDeviceWindow);}
    return result;
}
static unsigned constant_calls=0;
static bool integer_movies=true;
static D3DVIEWPORT8 last_movie_viewport={};
struct DrawAdjustment {
    IDirect3DDevice8* device;
    bool constants_changed=false,viewport_changed=false;
    float constants[4]={};D3DVIEWPORT8 viewport={};
    DWORD multisample=0,stage_values[4]={};bool multisample_changed=false,stage_changed[4]={};
    DWORD fog=0;bool fog_changed=false;
    const D3DTEXTURESTAGESTATETYPE stage_types[4]={D3DTSS_ADDRESSU,D3DTSS_ADDRESSV,D3DTSS_MINFILTER,D3DTSS_MAGFILTER};
    explicit DrawAdjustment(IDirect3DDevice8* value,bool flat_screen=false,bool background_layer=false):device(value) {
        if(!wide&&!osm::movie_files.load()&&!sprite_fix_mode&&!background_layer)return;
        if(!on_backbuffer(device))return;
        if(background_layer&&SUCCEEDED(IDirect3DDevice8_GetRenderState(device,D3DRS_FOGENABLE,&fog))&&fog)
            fog_changed=SUCCEEDED(IDirect3DDevice8_SetRenderState(device,D3DRS_FOGENABLE,FALSE));
        if(osm::movie_files.load()) {
            if(FAILED(original_get_viewport(device,&viewport)))return;
            RECT client={};GetClientRect(game_window,&client);
            const unsigned output_width=presentation_rect.right-presentation_rect.left,output_height=presentation_rect.bottom-presentation_rect.top;
            const float pixels=std::max(output_width/static_cast<float>(std::max(1L,client.right)),output_height/static_cast<float>(std::max(1L,client.bottom)));
            float factor=std::min(output_width/640.f,output_height/480.f)/pixels;
            if(integer_movies&&factor>=1)factor=std::floor(factor);
            const unsigned width=static_cast<unsigned>(640*factor*pixels),height=static_cast<unsigned>(480*factor*pixels);
            D3DVIEWPORT8 movie={presentation_rect.left+(output_width-width)/2,presentation_rect.top+(output_height-height)/2,width,height,0,1};
            last_movie_viewport=movie;
            if(SUCCEEDED(original_set_viewport(device,&movie)))viewport_changed=true;
        } else {
            DWORD shader=0;current_shader(device,&shader);
            const bool sprite=clip_shaders.count(shader)||flat_screen;
            if(sprite&&sprite_fix_mode) {
                if((sprite_fix_mode&1)&&SUCCEEDED(IDirect3DDevice8_GetRenderState(device,D3DRS_MULTISAMPLEANTIALIAS,&multisample))&&multisample)
                    multisample_changed=SUCCEEDED(IDirect3DDevice8_SetRenderState(device,D3DRS_MULTISAMPLEANTIALIAS,FALSE));
                for(unsigned i=0;i<4;++i) {
                    if(!(sprite_fix_mode&(i<2?2:4)))continue;
                    if(FAILED(IDirect3DDevice8_GetTextureStageState(device,0,stage_types[i],&stage_values[i])))continue;
                    const DWORD value=i<2?D3DTADDRESS_CLAMP:stage_values[i]==D3DTEXF_POINT?D3DTEXF_POINT:D3DTEXF_LINEAR;
                    if(value!=stage_values[i])stage_changed[i]=SUCCEEDED(IDirect3DDevice8_SetTextureStageState(device,0,stage_types[i],value));
                }
            }
            if(wide&&projection_shaders.count(shader)) {
                apply_projection_correction(device);
            }
            if(wide&&clip_shaders.count(shader)&&SUCCEEDED(current_constant0(device,constants))) {
                float corrected[4];std::memcpy(corrected,constants,sizeof(corrected));corrected[0]*=scale;corrected[1]*=vertical_scale;
                constants_changed=SUCCEEDED(real_shader_constant(device,0,corrected,1));
            }
        }
    }
    ~DrawAdjustment(){if(constants_changed)real_shader_constant(device,0,constants,1);if(viewport_changed)original_set_viewport(device,&viewport);if(multisample_changed)IDirect3DDevice8_SetRenderState(device,D3DRS_MULTISAMPLEANTIALIAS,multisample);if(fog_changed)IDirect3DDevice8_SetRenderState(device,D3DRS_FOGENABLE,fog);for(unsigned i=0;i<4;++i)if(stage_changed[i])IDirect3DDevice8_SetTextureStageState(device,0,stage_types[i],stage_values[i]);}
};

// Legacy pretransformed sky geometry can cross the projection horizon and
// contain signed RHW. D3D11 clips negative homogeneous W, unlike the intended
// already-projected raster. Keep raster W positive and carry signed perspective
// in projected texture coordinates instead: (u*rhw,v*rhw,rhw).
struct SkyProjectiveCopy {
    IDirect3DDevice8* device;
    std::vector<unsigned char> vertices;
    DWORD shader=0,flags=0;D3DMATRIX matrix={};
    bool changed_shader=false,changed_flags=false,changed_matrix=false;
    UINT stride=0;
    explicit SkyProjectiveCopy(IDirect3DDevice8* value,const void* data,UINT count,UINT source_stride,bool background):device(value) {
        if(!background||!data||!count||source_stride<16||FAILED(current_shader(device,&shader)))return;
        bool signed_rhw=false;
        for(UINT i=0;i<count;++i){float q;std::memcpy(&q,static_cast<const BYTE*>(data)+i*source_stride+12,4);if(q<0)signed_rhw=true;}
        if(!signed_rhw)return;
        const bool textured=shader==0x1c4&&source_stride>=32;
        const bool colored=shader==0x44&&source_stride>=20;
        if(!textured&&!colored)return;
        stride=textured?36:source_stride;
        vertices.resize(static_cast<size_t>(count)*stride);
        for(UINT i=0;i<count;++i) {
            auto* target=vertices.data()+static_cast<size_t>(i)*stride;
            const auto* source=static_cast<const BYTE*>(data)+static_cast<size_t>(i)*source_stride;
            std::memcpy(target,source,textured?24:source_stride);
            const float one=1;std::memcpy(target+12,&one,4);
            if(textured){float uv[2],q;std::memcpy(uv,source+24,8);std::memcpy(&q,source+12,4);float projected[3]={uv[0]*q,uv[1]*q,q};std::memcpy(target+24,projected,12);}
        }
        if(textured) {
            if(FAILED(IDirect3DDevice8_GetTextureStageState(device,0,D3DTSS_TEXTURETRANSFORMFLAGS,&flags))||FAILED(IDirect3DDevice8_GetTransform(device,D3DTS_TEXTURE0,&matrix))){vertices.clear();return;}
            D3DMATRIX identity={};identity._11=identity._22=identity._33=identity._44=1;
            changed_matrix=SUCCEEDED(IDirect3DDevice8_SetTransform(device,D3DTS_TEXTURE0,&identity));
            if(changed_matrix)changed_flags=SUCCEEDED(IDirect3DDevice8_SetTextureStageState(device,0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_COUNT3|D3DTTFF_PROJECTED));
            if(changed_flags)changed_shader=SUCCEEDED(IDirect3DDevice8_SetVertexShader(device,shader|D3DFVF_TEXCOORDSIZE3(0)));
            if(!changed_shader){restore();vertices.clear();}
        }
    }
    void restore(){if(changed_shader)IDirect3DDevice8_SetVertexShader(device,shader);if(changed_flags)IDirect3DDevice8_SetTextureStageState(device,0,D3DTSS_TEXTURETRANSFORMFLAGS,flags);if(changed_matrix)IDirect3DDevice8_SetTransform(device,D3DTS_TEXTURE0,&matrix);changed_shader=changed_flags=changed_matrix=false;}
    ~SkyProjectiveCopy(){restore();}
    const void* data(const void* original)const{return vertices.empty()?original:vertices.data();}
    UINT vertex_stride(UINT original)const{return vertices.empty()?original:stride;}
};

static HRESULT WINAPI shader_constant(IDirect3DDevice8* device,DWORD first,const void* data,DWORD count) {
    if(first<=95&&first+count>95)correction_dirty=true;
    if(data && constant_calls++<24) {
        const float* values=static_cast<const float*>(data);
        FILE* file=std::fopen(log_path,"a");
        if(file) {std::fprintf(file,"constants register=%lu count=%lu first4=%.6f,%.6f,%.6f,%.6f\n",first,count,values[0],values[1],values[2],values[3]);std::fclose(file);}
    }
    const HRESULT result=real_shader_constant(device,first,data,count);
    if(SUCCEEDED(result)&&first==0&&count&&data){select_state_device(device);if(state_recording)constant0_known=false;else{std::memcpy(cached_constant0,data,16);constant0_known=true;}}
    return result;
}

template<class T> static void hook(T* slot, T replacement, T& original) {
    if (osm::hook_decision(*slot,replacement,original)!=osm::HookDecision::Install) return;
    DWORD protection;
    if (!VirtualProtect(slot,sizeof(T),PAGE_READWRITE,&protection)) return;
    if(!original)original=*slot; *slot=replacement;
    VirtualProtect(slot,sizeof(T),protection,&protection);
}

struct MirroredBuffer {
    osm::BufferMirror data;
    const BYTE* mapped=nullptr;
    bool writing=false;
    explicit MirroredBuffer(UINT size):data(size){}
};
static std::unordered_map<void*,std::unique_ptr<MirroredBuffer>> buffer_mirrors;
static size_t mirror_bytes=0;
static unsigned long long mirror_hits=0,mirror_misses=0;
static constexpr size_t mirror_budget=32u*1024u*1024u;
static MirroredBuffer* mirror_for(void* object){const auto item=buffer_mirrors.find(object);return item==buffer_mirrors.end()?nullptr:item->second.get();}
static bool track_buffer(void* object,UINT size) {
    if(mirror_for(object))return true;
    if(!size||size>8u*1024u*1024u||buffer_mirrors.size()>=512||2ull*size>mirror_budget-mirror_bytes)return false;
    try {buffer_mirrors.emplace(object,std::make_unique<MirroredBuffer>(size));mirror_bytes+=2ull*size;return true;}
    catch(const std::bad_alloc&){return false;}
}
static void begin_mirror(void* object,UINT offset,UINT size,DWORD flags,const BYTE* pointer) {
    if(auto* mirror=mirror_for(object)) {
        mirror->writing=!(flags&D3DLOCK_READONLY)&&pointer&&mirror->data.begin(offset,size,(flags&D3DLOCK_DISCARD)!=0);
        mirror->mapped=mirror->writing?pointer:nullptr;
    }
}
static void stage_mirror(void* object){if(auto* mirror=mirror_for(object))if(mirror->writing)mirror->data.stage(mirror->mapped);}
static void finish_mirror(void* object,bool success){if(auto* mirror=mirror_for(object)){if(mirror->writing)mirror->data.finish(success);mirror->writing=false;mirror->mapped=nullptr;}}
static void release_mirror(void* object){const auto item=buffer_mirrors.find(object);if(item!=buffer_mirrors.end()){mirror_bytes-=2*item->second->data.size();buffer_mirrors.erase(item);}}
using VertexLockFn=HRESULT (WINAPI *)(IDirect3DVertexBuffer8*,UINT,UINT,BYTE**,DWORD);
using VertexUnlockFn=HRESULT (WINAPI *)(IDirect3DVertexBuffer8*);
using VertexReleaseFn=ULONG (WINAPI *)(IDirect3DVertexBuffer8*);
using IndexLockFn=HRESULT (WINAPI *)(IDirect3DIndexBuffer8*,UINT,UINT,BYTE**,DWORD);
using IndexUnlockFn=HRESULT (WINAPI *)(IDirect3DIndexBuffer8*);
using IndexReleaseFn=ULONG (WINAPI *)(IDirect3DIndexBuffer8*);
static VertexLockFn original_vertex_lock;
static VertexUnlockFn original_vertex_unlock;
static VertexReleaseFn original_vertex_release;
static IndexLockFn original_index_lock;
static IndexUnlockFn original_index_unlock;
static IndexReleaseFn original_index_release;
static HRESULT WINAPI vertex_lock(IDirect3DVertexBuffer8* buffer,UINT offset,UINT size,BYTE** pointer,DWORD flags){const auto result=original_vertex_lock(buffer,offset,size,pointer,flags);if(SUCCEEDED(result)&&pointer)begin_mirror(buffer,offset,size,flags,*pointer);return result;}
static HRESULT WINAPI vertex_unlock(IDirect3DVertexBuffer8* buffer){stage_mirror(buffer);const auto result=original_vertex_unlock(buffer);finish_mirror(buffer,SUCCEEDED(result));return result;}
static ULONG WINAPI vertex_release(IDirect3DVertexBuffer8* buffer){const auto result=original_vertex_release(buffer);if(!result)release_mirror(buffer);return result;}
static HRESULT WINAPI index_lock(IDirect3DIndexBuffer8* buffer,UINT offset,UINT size,BYTE** pointer,DWORD flags){const auto result=original_index_lock(buffer,offset,size,pointer,flags);if(SUCCEEDED(result)&&pointer)begin_mirror(buffer,offset,size,flags,*pointer);return result;}
static HRESULT WINAPI index_unlock(IDirect3DIndexBuffer8* buffer){stage_mirror(buffer);const auto result=original_index_unlock(buffer);finish_mirror(buffer,SUCCEEDED(result));return result;}
static ULONG WINAPI index_release(IDirect3DIndexBuffer8* buffer){const auto result=original_index_release(buffer);if(!result)release_mirror(buffer);return result;}
static void observe_vertex_buffer(IDirect3DVertexBuffer8* buffer,UINT size) {
    if(!track_buffer(buffer,size))return;
    auto* table=const_cast<IDirect3DVertexBuffer8Vtbl*>(buffer->lpVtbl);
    hook(&table->Lock,vertex_lock,original_vertex_lock);hook(&table->Unlock,vertex_unlock,original_vertex_unlock);hook(&table->Release,vertex_release,original_vertex_release);
}
static void observe_index_buffer(IDirect3DIndexBuffer8* buffer,UINT size) {
    if(!track_buffer(buffer,size))return;
    auto* table=const_cast<IDirect3DIndexBuffer8Vtbl*>(buffer->lpVtbl);
    hook(&table->Lock,index_lock,original_index_lock);hook(&table->Unlock,index_unlock,original_index_unlock);hook(&table->Release,index_release,original_index_release);
}
using CreateVertexBufferFn=HRESULT (WINAPI *)(IDirect3DDevice8*,UINT,DWORD,DWORD,D3DPOOL,IDirect3DVertexBuffer8**);
using CreateIndexBufferFn=HRESULT (WINAPI *)(IDirect3DDevice8*,UINT,DWORD,D3DFORMAT,D3DPOOL,IDirect3DIndexBuffer8**);
static CreateVertexBufferFn original_create_vertex_buffer;
static CreateIndexBufferFn original_create_index_buffer;
static HRESULT WINAPI create_vertex_buffer(IDirect3DDevice8* device,UINT length,DWORD usage,DWORD fvf,D3DPOOL pool,IDirect3DVertexBuffer8** output) {
    const auto result=original_create_vertex_buffer(device,length,usage,fvf,pool,output);
    if(SUCCEEDED(result)&&output&&*output&&(osm::screen_fvf(fvf)||(!fvf&&length<=65536)))observe_vertex_buffer(*output,length);
    return result;
}
static HRESULT WINAPI create_index_buffer(IDirect3DDevice8* device,UINT length,DWORD usage,D3DFORMAT format,D3DPOOL pool,IDirect3DIndexBuffer8** output) {
    const auto result=original_create_index_buffer(device,length,usage,format,pool,output);
    if(SUCCEEDED(result)&&output&&*output&&length<=65536)observe_index_buffer(*output,length);
    return result;
}

static bool on_backbuffer(IDirect3DDevice8* device) {
    (void)device;return target_is_backbuffer;
}

static HRESULT WINAPI set_transform(IDirect3DDevice8* device,D3DTRANSFORMSTATETYPE state,const D3DMATRIX* matrix) {
    if(matrix && state==D3DTS_PROJECTION) {
        ++projections;
        if(projections<=3) {
            FILE* file=std::fopen(log_path,"a");
            if(file){std::fprintf(file,"projection _11=%.6f _22=%.6f _34=%.6f _44=%.6f\n",matrix->_11,matrix->_22,matrix->_34,matrix->_44);std::fclose(file);}
        }
        original_projection=*matrix;have_projection=true;
        if(wide && !osm::camera_hooks_active && osm::perspective(matrix->_34,matrix->_44) && on_backbuffer(device)) {
            D3DMATRIX corrected=*matrix;corrected._11*=scale;corrected._22*=vertical_scale;
            return real_transform(device,state,&corrected);
        }
    }
    return real_transform(device,state,matrix);
}

static HRESULT WINAPI get_transform(IDirect3DDevice8* device,D3DTRANSFORMSTATETYPE state,D3DMATRIX* matrix) {
    // The application always sees the matrix it supplied, keeping its calculations unchanged.
    if(wide && matrix && state==D3DTS_PROJECTION && have_projection) {*matrix=original_projection;return D3D_OK;}
    return real_get_transform(device,state,matrix);
}

static bool copy_hud(IDirect3DDevice8* device,const void* data,UINT vertices,UINT stride,std::vector<unsigned char>& copy,bool hud=true,bool* flat=nullptr,bool* background_layer=nullptr) {
    DWORD fvf=0;
    if(!data || stride<16 || stride>256 || vertices>100000 || FAILED(current_shader(device,&fvf)) ||
       !screen_format(fvf) || !on_backbuffer(device)) return false;
    D3DVIEWPORT8 viewport;
    if(FAILED(IDirect3DDevice8_GetViewport(device,&viewport)))return false;
    if(hud)for(UINT i=0;i<vertices&&hud;++i){float z,rhw;const auto* bytes=static_cast<const unsigned char*>(data)+i*stride;std::memcpy(&z,bytes+8,4);std::memcpy(&rhw,bytes+12,4);hud=osm::overlay_depth(z,rhw);}
    if(flat)*flat=hud;
    DWORD depth_enabled=TRUE;
    bool background=!hud&&SUCCEEDED(IDirect3DDevice8_GetRenderState(device,D3DRS_ZENABLE,&depth_enabled))&&!depth_enabled;
    if(background)for(UINT i=0;i<vertices&&background;++i){float z,rhw;const auto* bytes=static_cast<const unsigned char*>(data)+i*stride;std::memcpy(&z,bytes+8,4);std::memcpy(&rhw,bytes+12,4);background=osm::background_depth(z,rhw);}
    if(background_layer)*background_layer=background;
    const size_t length=static_cast<size_t>(vertices)*stride;
    copy.resize(length);std::memcpy(copy.data(),data,length);
    for(UINT i=0;i<vertices;++i) {
        if(background){const float far_depth=1.f;std::memcpy(copy.data()+i*stride+8,&far_depth,4);}
        float x;std::memcpy(&x,copy.data()+i*stride,sizeof(x));
        if(wide&&hud)x=osm::hud_x(x,static_cast<float>(viewport.X),static_cast<float>(viewport.Width),scale);
        x=osm::raster_coordinate(x,raster_x,raster_offset_x);
        std::memcpy(copy.data()+i*stride,&x,sizeof(x));
        float y;std::memcpy(&y,copy.data()+i*stride+4,sizeof(y));y=osm::raster_coordinate(y,raster_y,raster_offset_y);std::memcpy(copy.data()+i*stride+4,&y,sizeof(y));
    }
    ++hud_draws;return true;
}

static UINT vertex_count(D3DPRIMITIVETYPE type,UINT count) {
    switch(type) {
        case D3DPT_POINTLIST:return count;
        case D3DPT_LINELIST:return count*2;
        case D3DPT_LINESTRIP:return count+1;
        case D3DPT_TRIANGLELIST:return count*3;
        case D3DPT_TRIANGLESTRIP:case D3DPT_TRIANGLEFAN:return count+2;
        default:return 0;
    }
}

static void probe_screen_vertices(IDirect3DDevice8* device,UINT start,UINT count) {
    static unsigned probes=0;
    if(probes>=12||!trace_screen_vertices)return;
    DWORD fvf=0;if(FAILED(current_shader(device,&fvf))||!screen_format(fvf))return;
    IDirect3DVertexBuffer8* buffer=nullptr;UINT stride=0;
    if(FAILED(IDirect3DDevice8_GetStreamSource(device,0,&buffer,&stride))||!buffer)return;
    D3DVERTEXBUFFER_DESC desc={};IDirect3DVertexBuffer8_GetDesc(buffer,&desc);
    BYTE* bytes=nullptr;HRESULT status=D3DERR_INVALIDCALL;
    const UINT vertices=std::min(count,6u);
    if(stride>=16&&stride<=256&&vertices&&start<=desc.Size/stride&&vertices<=desc.Size/stride-start)
        status=IDirect3DVertexBuffer8_Lock(buffer,start*stride,vertices*stride,&bytes,D3DLOCK_READONLY);
    FILE* file=std::fopen(log_path,"a");
    if(file){std::fprintf(file,"SCREEN_VB fvf=%lx stride=%u usage=%lx start=%u count=%u lock=%lx",fvf,stride,desc.Usage,start,count,status);
        if(SUCCEEDED(status))for(UINT i=0;i<vertices;++i){const auto* v=reinterpret_cast<const float*>(bytes+i*stride);std::fprintf(file," [%.3f,%.3f,z=%.9g,rhw=%.9g",v[0],v[1],v[2],v[3]);if(stride>=32)std::fprintf(file,",uv=%.9g,%.9g",v[6],v[7]);std::fputc(']',file);}
        std::fputc('\n',file);std::fclose(file);}
    if(SUCCEEDED(status))IDirect3DVertexBuffer8_Unlock(buffer);
    IDirect3DVertexBuffer8_Release(buffer);++probes;
}

#include "world_effects.h"
static bool world_stencil_draw(IDirect3DDevice8* device,D3DPRIMITIVETYPE type,UINT count,const void* data,UINT stride) {
    if(data!=reinterpret_cast<const void*>(0x65ba20)||type!=D3DPT_TRIANGLESTRIP||count!=2||stride!=20||!on_backbuffer(device))return false;
    static const bool supported=[] {
        const auto* base=reinterpret_cast<const unsigned char*>(GetModuleHandleA(nullptr));
        if(reinterpret_cast<std::uintptr_t>(base)!=0x400000||!GetPrivateProfileIntA("Graphics","VerifiedExecutable",0,config_path))return false;
        const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<0||dos->e_lfanew>0x1000-static_cast<LONG>(sizeof(IMAGE_NT_HEADERS32)))return false;
        const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS32*>(base+dos->e_lfanew);
        return nt->Signature==IMAGE_NT_SIGNATURE&&nt->OptionalHeader.Magic==IMAGE_NT_OPTIONAL_HDR32_MAGIC&&
            osm::world_stencil_signature(base,nt->OptionalHeader.SizeOfImage,true);
    }();
    DWORD format=0,stencil=FALSE;
    if(!supported||FAILED(current_shader(device,&format))||FAILED(IDirect3DDevice8_GetRenderState(device,D3DRS_STENCILENABLE,&stencil)))return false;
    return osm::world_stencil_composite(supported,reinterpret_cast<std::uintptr_t>(data),format,type,count,stride,stencil!=FALSE,data,logical_width,logical_height);
}
static HRESULT WINAPI draw_up(IDirect3DDevice8* device,D3DPRIMITIVETYPE type,UINT count,const void* data,UINT stride) {
    if(osm::title_background_drawing)return real_draw_up(device,type,count,data,stride);
    ++up_draws;std::vector<unsigned char> copy;
    bool flat=false,background=false;
    if(up_draws<=3){DWORD fvf=0;IDirect3DDevice8_GetVertexShader(device,&fvf);FILE* file=std::fopen(log_path,"a");if(file){std::fprintf(file,"draw_up fvf=%lx stride=%u\n",fvf,stride);std::fclose(file);}}
    const UINT vertices=count<=100000?vertex_count(type,count):0;
    const bool world_stencil=wide&&world_stencil_draw(device,type,count,data,stride);
    osm::WorldStencilVertex stencil_vertices[4];
    if(world_stencil){std::memcpy(stencil_vertices,data,sizeof(stencil_vertices));osm::world_stencil_edges(stencil_vertices,logical_width,logical_height);data=stencil_vertices;}
    const bool hud=!world_stencil;
    if(vertices && copy_hud(device,data,vertices,stride,copy,hud,&flat,&background))data=copy.data();
    DrawAdjustment adjustment(device,flat,background);
    SkyProjectiveCopy projective(device,data,vertices,stride,background);
    const HRESULT result=real_draw_up(device,type,count,projective.data(data),projective.vertex_stride(stride));
    if(capture_sky_pending&&!copy.empty()){DWORD fvf=0;current_shader(device,&fvf);capture_screen_layer(device,data,vertices,stride,fvf,flat,background);}
    return result;
}

static HRESULT WINAPI draw_indexed_up(IDirect3DDevice8* device,D3DPRIMITIVETYPE type,UINT minimum,UINT vertices,UINT count,const void* indices,D3DFORMAT format,const void* data,UINT stride) {
    ++up_draws;std::vector<unsigned char> copy;
    bool flat=false,background=false;
    // Include the leading vertices referenced by min_vertex_idx; leave all indices intact.
    if(minimum<=100000 && vertices<=100000-minimum && copy_hud(device,data,minimum+vertices,stride,copy,true,&flat,&background))data=copy.data();
    DrawAdjustment adjustment(device,flat,background);
    SkyProjectiveCopy projective(device,data,minimum+vertices,stride,background);
    return real_draw_indexed_up(device,type,minimum,vertices,count,indices,format,projective.data(data),projective.vertex_stride(stride));
}

struct ScreenBufferCopy {
    IDirect3DDevice8* device;
    IDirect3DVertexBuffer8* buffer=nullptr;
    UINT stride=0;
    bool flat=false;
    bool background=false;
    DWORD format=0;
    std::vector<unsigned char> vertices;
    explicit ScreenBufferCopy(IDirect3DDevice8* value):device(value){}
    ~ScreenBufferCopy(){if(buffer)IDirect3DVertexBuffer8_Release(buffer);}
    bool prepare(UINT first,UINT count) {
        DWORD fvf=0;
        if(!count||count>100000||!on_backbuffer(device)||FAILED(current_shader(device,&fvf))||!screen_format(fvf))return false;
        format=fvf;
        if(FAILED(IDirect3DDevice8_GetStreamSource(device,0,&buffer,&stride))||!buffer||stride<16||stride>256)return false;
        D3DVERTEXBUFFER_DESC desc={};if(FAILED(IDirect3DVertexBuffer8_GetDesc(buffer,&desc)))return false;
        if(first>desc.Size/stride||count>desc.Size/stride-first)return false;
        if(auto* mirror=mirror_for(buffer))if(const auto* data=mirror->data.read(first*stride,count*stride)) {
            ++mirror_hits;return copy_hud(device,data,count,stride,vertices,true,&flat,&background);
        }
        ++mirror_misses;
        if(desc.Usage&D3DUSAGE_WRITEONLY) {
            observe_vertex_buffer(buffer,desc.Size);
            static unsigned missing=0;if(missing++<4){FILE* file=std::fopen(log_path,"a");if(file){std::fprintf(file,"SCREEN_BUFFER_FALLBACK stage=missing-write-mirror bytes=%u\n",desc.Size);std::fclose(file);}}
            return false;
        }
        BYTE* bytes=nullptr;
        LARGE_INTEGER begin,end;QueryPerformanceCounter(&begin);
        const HRESULT locked=IDirect3DVertexBuffer8_Lock(buffer,first*stride,count*stride,&bytes,D3DLOCK_READONLY);
        QueryPerformanceCounter(&end);screen_lock_ticks+=end.QuadPart-begin.QuadPart;++screen_locks;
        if(FAILED(locked)) {
            static unsigned failures=0;
            if(failures++<4){FILE* file=std::fopen(log_path,"a");if(file){std::fprintf(file,"SCREEN_BUFFER_FALLBACK stage=readonly-lock result=0x%lx usage=0x%lx\n",locked,desc.Usage);std::fclose(file);}}
            return false;
        }
        // World-space projected layers (sky and shadows) must cover the output,
        // whereas flat HUD layers keep their original proportions.
        const bool ready=copy_hud(device,bytes,count,stride,vertices,true,&flat,&background);
        IDirect3DVertexBuffer8_Unlock(buffer);
        return ready;
    }
    void restore(){IDirect3DDevice8_SetStreamSource(device,0,buffer,stride);}
};

static void capture_screen_layer(IDirect3DDevice8* device,const void* vertices,UINT count,UINT stride,DWORD format,bool flat,bool background) {
    if(!capture_sky_pending||flat||!vertices||!count||stride<16||!screen_format(format))return;
    char tag[32];std::snprintf(tag,sizeof(tag),"sky-layer-%u",++capture_sky_layer);
    if(capture_sky_layer<=6) {
        const bool ended=SUCCEEDED(IDirect3DDevice8_EndScene(device));
        capture_backbuffer(device,tag);
        if(ended)IDirect3DDevice8_BeginScene(device);
    }
    FILE* file=std::fopen(log_path,"a");
    if(file) {
        std::fprintf(file,"SKY_LAYER frame=%lu layer=%u fvf=%lx vertices=%u background=%d",presents,capture_sky_layer,format,count,background);
        float low[4]={INFINITY,INFINITY,INFINITY,INFINITY},high[4]={-INFINITY,-INFINITY,-INFINITY,-INFINITY};
        for(UINT i=0;i<count;++i)for(UINT j=0;j<4;++j){float value;std::memcpy(&value,static_cast<const BYTE*>(vertices)+i*stride+j*4,4);low[j]=std::min(low[j],value);high[j]=std::max(high[j],value);}
        std::fprintf(file," x=%.3f:%.3f y=%.3f:%.3f z=%.7f:%.7f rhw=%.9f:%.9f",low[0],high[0],low[1],high[1],low[2],high[2],low[3],high[3]);
        if(stride>=24){DWORD diffuse=0,specular=0;std::memcpy(&diffuse,static_cast<const BYTE*>(vertices)+16,4);std::memcpy(&specular,static_cast<const BYTE*>(vertices)+20,4);std::fprintf(file," diffuse=%08lx specular=%08lx",diffuse,specular);}
        for(auto type:{D3DRS_ZENABLE,D3DRS_FOGENABLE,D3DRS_ALPHABLENDENABLE,D3DRS_SRCBLEND,D3DRS_DESTBLEND,D3DRS_FOGTABLEMODE,D3DRS_FOGVERTEXMODE,D3DRS_CLIPPING}){DWORD value=0;IDirect3DDevice8_GetRenderState(device,type,&value);std::fprintf(file," rs%u=%lx",type,value);}
        for(auto type:{D3DTSS_COLOROP,D3DTSS_COLORARG1,D3DTSS_COLORARG2,D3DTSS_ALPHAOP,D3DTSS_ALPHAARG1,D3DTSS_ADDRESSU,D3DTSS_ADDRESSV}){DWORD value=0;IDirect3DDevice8_GetTextureStageState(device,0,type,&value);std::fprintf(file," ts%u=%lx",type,value);}
        IDirect3DBaseTexture8* texture=nullptr;IDirect3DDevice8_GetTexture(device,0,&texture);
        if(texture){if(IDirect3DBaseTexture8_GetType(texture)==D3DRTYPE_TEXTURE){D3DSURFACE_DESC desc={};IDirect3DTexture8_GetLevelDesc(reinterpret_cast<IDirect3DTexture8*>(texture),0,&desc);std::fprintf(file," texture=%ux%u format=%u",desc.Width,desc.Height,desc.Format);}IDirect3DBaseTexture8_Release(texture);}
        std::fputc('\n',file);std::fclose(file);
    }
    if(capture_sky_layer>=16)capture_sky_pending=false;
}
static void capture_sky_layer_after_draw(IDirect3DDevice8* device,const ScreenBufferCopy& screen) {
    if(!screen.vertices.empty())capture_screen_layer(device,screen.vertices.data(),static_cast<UINT>(screen.vertices.size()/screen.stride),screen.stride,screen.format,screen.flat,screen.background);
}

static HRESULT WINAPI draw(IDirect3DDevice8* device,D3DPRIMITIVETYPE type,UINT start,UINT count) {
    probe_screen_vertices(device,start,vertex_count(type,count));
    ScreenBufferCopy screen(device);
    if(count<=33333&&screen.prepare(start,vertex_count(type,count))) {
        DrawAdjustment adjustment(device,screen.flat,screen.background);
        SkyProjectiveCopy projective(device,screen.vertices.data(),vertex_count(type,count),screen.stride,screen.background);
        const HRESULT result=real_draw_up(device,type,count,projective.data(screen.vertices.data()),projective.vertex_stride(screen.stride));
        capture_sky_layer_after_draw(device,screen);
        screen.restore();return result;
    }
    ++vb_draws;DrawAdjustment adjustment(device);return real_draw(device,type,start,count);
}
static HRESULT WINAPI draw_indexed(IDirect3DDevice8* device,D3DPRIMITIVETYPE type,UINT minimum,UINT vertices,UINT start,UINT count) {
    if(trace_screen_vertices) {
        IDirect3DIndexBuffer8* indices=nullptr;UINT base=0;
        if(SUCCEEDED(IDirect3DDevice8_GetIndices(device,&indices,&base))&&indices){probe_screen_vertices(device,base+minimum,vertices);IDirect3DIndexBuffer8_Release(indices);}
    }
    DWORD fvf=0;
    if(SUCCEEDED(current_shader(device,&fvf))&&screen_format(fvf)&&count<=33333) {
        IDirect3DIndexBuffer8* indices=nullptr;UINT base=0;
        if(SUCCEEDED(IDirect3DDevice8_GetIndices(device,&indices,&base))&&indices) {
            ScreenBufferCopy screen(device);D3DINDEXBUFFER_DESC desc={};
            if(base<=UINT_MAX-minimum&&screen.prepare(base+minimum,vertices)&&SUCCEEDED(IDirect3DIndexBuffer8_GetDesc(indices,&desc))) {
                const UINT size=desc.Format==D3DFMT_INDEX16?2:desc.Format==D3DFMT_INDEX32?4:0;
                const UINT total=vertex_count(type,count);
                BYTE* bytes=nullptr;
                const unsigned char* source=nullptr;bool locked=false;
                if(size&&start<=desc.Size/size&&total<=desc.Size/size-start) {
                    if(auto* mirror=mirror_for(indices))source=mirror->data.read(start*size,total*size);
                    if(source)++mirror_hits;
                    else {
                        ++mirror_misses;
                        if(!(desc.Usage&D3DUSAGE_WRITEONLY)&&SUCCEEDED(IDirect3DIndexBuffer8_Lock(indices,start*size,total*size,&bytes,D3DLOCK_READONLY))){source=bytes;locked=true;}
                        else observe_index_buffer(indices,desc.Size);
                    }
                }
                if(source) {
                    std::vector<unsigned char> rebased(total*size);bool valid=true;
                    for(UINT i=0;i<total;++i){DWORD index=0;std::memcpy(&index,source+i*size,size);if(index<minimum||index-minimum>=vertices){valid=false;break;}index-=minimum;std::memcpy(rebased.data()+i*size,&index,size);}
                    if(locked)IDirect3DIndexBuffer8_Unlock(indices);
                    if(valid) {
                        DrawAdjustment adjustment(device,screen.flat,screen.background);
                        SkyProjectiveCopy projective(device,screen.vertices.data(),vertices,screen.stride,screen.background);
                        const HRESULT result=real_draw_indexed_up(device,type,0,vertices,count,rebased.data(),desc.Format,projective.data(screen.vertices.data()),projective.vertex_stride(screen.stride));
                        capture_sky_layer_after_draw(device,screen);
                        screen.restore();IDirect3DDevice8_SetIndices(device,indices,base);IDirect3DIndexBuffer8_Release(indices);return result;
                    }
                }
            }
            IDirect3DIndexBuffer8_Release(indices);
        }
    }
    ++vb_draws;DrawAdjustment adjustment(device);return real_draw_indexed(device,type,minimum,vertices,start,count);
}

static void update_window_aspect(IDirect3DDevice8* device) {
    if(!wide||!auto_window_aspect||display_mode==2)return;
    RECT client={};if(!GetClientRect(game_window,&client)||client.right<1||client.bottom<1)return;
    const auto fit=osm::fit_rect(requested_width,requested_height,static_cast<double>(client.right)/client.bottom);
    RECT output={fit.x,fit.y,fit.x+fit.width,fit.y+fit.height};
    if(std::memcmp(&output,&presentation_rect,sizeof(output))==0)return;
    presentation_rect=output;
    raster_x=static_cast<float>(fit.width)/logical_width;raster_y=static_cast<float>(fit.height)/logical_height;
    raster_offset_x=static_cast<float>(fit.x);raster_offset_y=static_cast<float>(fit.y);
    const float aspect=static_cast<float>(fit.width)/fit.height;
    scale=std::min(1.f,(4.f/3.f)/aspect);vertical_scale=std::min(1.f,aspect/(4.f/3.f));
    osm::engine_aspect=aspect;
    D3DVIEWPORT8 viewport={static_cast<DWORD>(fit.x),static_cast<DWORD>(fit.y),static_cast<DWORD>(fit.width),static_cast<DWORD>(fit.height),0,1};
    original_set_viewport(device,&viewport);
    FILE* file=std::fopen(log_path,"a");if(file){std::fprintf(file,"WINDOW_ASPECT client=%ldx%ld roi=%dx%d+%d+%d correction=%.5f,%.5f\n",client.right,client.bottom,fit.width,fit.height,fit.x,fit.y,scale,vertical_scale);std::fclose(file);}
}

static HRESULT WINAPI present(IDirect3DDevice8* device,const RECT* source,const RECT* dest,HWND window,const RGNDATA* dirty) {
    osm::reset_title_frame();
    settings_key(VK_F1); // Also opens a late private test/controller page.
    settings_menu.set_controller(private_keyboard.controller_present());
    settings_menu.update(settings_key,(game_window&&GetForegroundWindow()==game_window)||private_keyboard.background_test_enabled(),osm::engine_gameplay());
    if(settings_menu.consume_changed()) {
        performance_hud.set_enabled(settings_menu.hud_enabled());
        overlay_user_scale=settings_menu.hud_scale()/100.f;
        osm::camera_zoom_index=settings_menu.zoom_index();
        osm::smooth_interpolation=settings_menu.smooth_enabled();
    }
    if(display_mode!=2) {
        const bool focused=game_window&&GetForegroundWindow()==game_window;
        const bool down=focused&&((GetAsyncKeyState(VK_F11)&0x8000)||
            ((GetAsyncKeyState(VK_MENU)&0x8000)&&(GetAsyncKeyState(VK_RETURN)&0x8000)));
        if(down&&!window_toggle_down)toggle_borderless(game_window);
        window_toggle_down=down;
    }
    update_window_aspect(device);
    performance_hud.begin(game_window);
    settings_menu.sync_hud(performance_hud.is_enabled());
    D3DVIEWPORT8 saved_viewport={},full={0,0,requested_width,requested_height,0,1};
    original_get_viewport(device,&saved_viewport);
    original_set_viewport(device,&full);
    D3DVIEWPORT8 content={static_cast<DWORD>(raster_offset_x),static_cast<DWORD>(raster_offset_y),static_cast<DWORD>(logical_width*raster_x),static_cast<DWORD>(logical_height*raster_y),0,1};
    if(osm::movie_files.load()&&last_movie_viewport.Width)content=last_movie_viewport;
    D3DRECT bars[4];DWORD bar_count=0;
    if(content.X)bars[bar_count++]={0,0,static_cast<LONG>(content.X),static_cast<LONG>(requested_height)};
    if(content.Y)bars[bar_count++]={0,0,static_cast<LONG>(requested_width),static_cast<LONG>(content.Y)};
    if(content.X+content.Width<requested_width)bars[bar_count++]={static_cast<LONG>(content.X+content.Width),0,static_cast<LONG>(requested_width),static_cast<LONG>(requested_height)};
    if(content.Y+content.Height<requested_height)bars[bar_count++]={0,static_cast<LONG>(content.Y+content.Height),static_cast<LONG>(requested_width),static_cast<LONG>(requested_height)};
    if(bar_count)IDirect3DDevice8_Clear(device,bar_count,bars,D3DCLEAR_TARGET,0xff000000,1,0);
    if(SUCCEEDED(IDirect3DDevice8_BeginScene(device))) {
        performance_hud.draw(device,overlay_draw);
        RECT client={};GetClientRect(game_window,&client);
        const float pixels=std::max((presentation_rect.right-presentation_rect.left)/static_cast<float>(std::max(1L,client.right)),(presentation_rect.bottom-presentation_rect.top)/static_cast<float>(std::max(1L,client.bottom)));
        settings_menu.draw(device,presentation_rect,pixels,real_draw_up);
        IDirect3DDevice8_EndScene(device);
    }
    original_set_viewport(device,&saved_viewport);
    ++presents;
    ++profile_frames;
    if(capture_frame && presents==capture_frame)capture_backbuffer(device);
    if(presents%60==0) {
        if(live_sprite_diagnostics)sprite_fix_mode=GetPrivateProfileIntA("Graphics","SpriteSeamFix",0,config_path);
        if(live_sprite_diagnostics){const bool enabled=GetPrivateProfileIntA("Graphics","DeviceStateCache",1,config_path)!=0;if(enabled!=state_cache_enabled){state_cache_enabled=enabled;invalidate_state_cache();}}
        char request[MAX_PATH];std::snprintf(request,MAX_PATH,"%sopensoccermania-capture.request",directory);
        if(GetFileAttributesA(request)!=INVALID_FILE_ATTRIBUTES){DeleteFileA(request);capture_backbuffer(device);}
        std::snprintf(request,MAX_PATH,"%sopensoccermania-sky-capture.request",directory);
        if(GetFileAttributesA(request)!=INVALID_FILE_ATTRIBUTES){DeleteFileA(request);capture_sky_layer=0;capture_sky_pending=true;}
    }
    if(presents==1 || presents%300==0) {
        osm::engine_report();
        FILE* file=std::fopen(log_path,"a");
        if(file) {std::fprintf(file,"presents=%lu projection_calls=%lu up_draws=%lu hud_corrected=%lu vertex_buffer_draws=%lu wide=%d scale=%.6f\n",presents,projections,up_draws,hud_draws,vb_draws,wide,scale);std::fclose(file);}
        const auto metrics=performance_hud.summary();
        file=std::fopen(log_path,"a");if(file){std::fprintf(file,"FRAME_METRICS fps=%.3f avg_ms=%.4f p95_ms=%.4f p99_ms=%.4f max_ms=%.4f present_ms=%.4f samples=%zu\n",metrics.fps,metrics.average_ms,metrics.p95_ms,metrics.p99_ms,metrics.max_ms,metrics.present_ms,metrics.samples);std::fclose(file);}
        file=std::fopen(log_path,"a");if(file){std::fprintf(file,"BRIDGE_WORK frames=%u state_cache=%d shader_queries=%llu shader_hits=%llu constant_queries=%llu constant_hits=%llu screen_locks=%llu screen_lock_ms_per_frame=%.6f mirror_hits=%llu mirror_misses=%llu mirror_bytes=%zu\n",profile_frames,state_cache_enabled,shader_queries,shader_hits,constant_queries,constant_hits,screen_locks,bridge_frequency.QuadPart?1000.*screen_lock_ticks/bridge_frequency.QuadPart/profile_frames:0,mirror_hits,mirror_misses,mirror_bytes);std::fclose(file);}
        mirror_hits=mirror_misses=0;
        shader_queries=shader_hits=constant_queries=constant_hits=screen_locks=0;screen_lock_ticks=0;profile_frames=0;
    }
    LARGE_INTEGER start,end,frequency;QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&start);
    const RECT* output=wide&&auto_window_aspect&&display_mode!=2&&!source?&presentation_rect:source;
    const HRESULT status=real_present(device,output,dest,window,dirty);
    QueryPerformanceCounter(&end);performance_hud.last_present=1000.0*(end.QuadPart-start.QuadPart)/frequency.QuadPart;
    if(display_mode!=2)ClipCursor(nullptr);
    return status;
}

static HRESULT WINAPI create_device(IDirect3D8* api,UINT adapter,D3DDEVTYPE type,HWND window,DWORD behavior,D3DPRESENT_PARAMETERS* params,IDirect3DDevice8** result) {
    if(!params)return D3DERR_INVALIDCALL;
    install_window_hooks(params->hDeviceWindow?params->hDeviceWindow:window);
    D3DPRESENT_PARAMETERS physical=*params;window_parameters(&physical);render_parameters(physical,*params);
    if(display_mode!=2) {
        D3DDISPLAYMODE desktop={};
        if(SUCCEEDED(IDirect3D8_GetAdapterDisplayMode(api,adapter,&desktop)))physical.BackBufferFormat=desktop.Format;
    }
    const HRESULT status=real_create(api,adapter,type,window,behavior,&physical,result);
    FILE* status_file=std::fopen(log_path,"a");if(status_file){std::fprintf(status_file,"CREATE_DEVICE result=0x%lx requested=%ux%u windowed=%d behavior=0x%lx adapter=%u\n",static_cast<unsigned long>(status),physical.BackBufferWidth,physical.BackBufferHeight,physical.Windowed,behavior,adapter);std::fclose(status_file);}
    if(SUCCEEDED(status) && result && *result) {
        cached_state_device=*result;invalidate_state_cache();state_recording=false;
        have_projection=false;
        correction_dirty=true;
        game_window=params->hDeviceWindow?params->hDeviceWindow:window;
        auto* table=const_cast<IDirect3DDevice8Vtbl*>((*result)->lpVtbl);
        hook(&table->Reset,reset,real_reset);
        hook(&table->GetBackBuffer,backbuffer,original_backbuffer);
        hook(&table->SetRenderTarget,render_target,original_render_target);
        hook(&table->Release,release_device,original_release_device);
        hook(&table->CreateVertexShader,create_shader,real_create_shader);
        hook(&table->CreateVertexBuffer,create_vertex_buffer,original_create_vertex_buffer);
        hook(&table->CreateIndexBuffer,create_index_buffer,original_create_index_buffer);
        hook(&table->DeleteVertexShader,delete_shader,real_delete_shader);
        hook(&table->SetVertexShader,set_shader,real_set_shader);
        hook(&table->BeginStateBlock,begin_state,real_begin_state);
        hook(&table->EndStateBlock,end_state,real_end_state);
        hook(&table->SetTransform,set_transform,real_transform);
        hook(&table->GetTransform,get_transform,real_get_transform);
        hook(&table->SetViewport,set_viewport,original_set_viewport);
        hook(&table->GetViewport,get_viewport,original_get_viewport);
        hook(&table->Present,present,real_present);
        hook(&table->DrawPrimitiveUP,draw_up,real_draw_up);
        hook(&table->DrawIndexedPrimitiveUP,draw_indexed_up,real_draw_indexed_up);
        hook(&table->DrawPrimitive,draw,real_draw);
        hook(&table->DrawIndexedPrimitive,draw_indexed,real_draw_indexed);
        hook(&table->SetVertexShaderConstant,shader_constant,real_shader_constant);
        hook(&table->ApplyStateBlock,apply_state,original_apply_state);
        target_is_backbuffer=true;
        osm::title_background.set_device(*result);
        IDirect3DSurface8* back=nullptr;if(SUCCEEDED(backbuffer(*result,0,D3DBACKBUFFER_TYPE_MONO,&back))&&back)IDirect3DSurface8_Release(back);
        D3DVIEWPORT8 viewport={0,0,logical_width,logical_height,0,1};set_viewport(*result,&viewport);
        FILE* file=std::fopen(log_path,"a");
        if(file) {std::fprintf(file,"device=%ux%u logical=%ux%u windowed=%d refresh=%u\n",physical.BackBufferWidth,physical.BackBufferHeight,params->BackBufferWidth,params->BackBufferHeight,physical.Windowed,physical.FullScreen_RefreshRateInHz);std::fclose(file);}
        window_appearance(params->hDeviceWindow?params->hDeviceWindow:window);
    }
    return status;
}

extern "C" IDirect3D8* WINAPI Direct3DCreate8(UINT sdk_version) {
    GetModuleFileNameA(self_module,directory,MAX_PATH);
    char* slash=std::strrchr(directory,'\\');if(!slash)return nullptr;slash[1]=0;
    std::snprintf(config_path,MAX_PATH,"%sopensoccermania.ini",directory);
    settings_menu.configure(directory,config_path);
    std::snprintf(log_path,MAX_PATH,"%sopensoccermania-render.log",directory);
    char mode[32];GetPrivateProfileStringA("Graphics","DisplayMode","Windowed",mode,sizeof(mode),config_path);
    display_mode=std::strcmp(mode,"Fullscreen")==0 ? 2 : std::strcmp(mode,"Borderless")==0 ? 1 : 0;
    performance_hud.configure(GetPrivateProfileIntA("Graphics","PerformanceHud",1,config_path)!=0);
    background_running=GetPrivateProfileIntA("Graphics","RunInBackground",1,config_path)!=0;
    capture_frame=GetPrivateProfileIntA("Diagnostics","CaptureFrame",0,config_path);
    trace_screen_vertices=GetPrivateProfileIntA("Diagnostics","TraceScreenVertices",0,config_path)!=0;
    live_sprite_diagnostics=GetPrivateProfileIntA("Diagnostics","LiveSpriteSettings",0,config_path)!=0;
    sprite_fix_mode=GetPrivateProfileIntA("Graphics","SpriteSeamFix",7,config_path);
    state_cache_enabled=GetPrivateProfileIntA("Graphics","DeviceStateCache",1,config_path)!=0;
    QueryPerformanceFrequency(&bridge_frequency);
    integer_movies=GetPrivateProfileIntA("Graphics","IntegerMovies",1,config_path)!=0;
    overlay_user_scale=std::clamp(GetPrivateProfileIntA("Graphics","OverlayScale",125,config_path)/100.0f,.5f,2.5f);
    if(!global_hooks_ready) {
        if(MH_Initialize()==MH_OK) {
            FILE* file=std::fopen(log_path,"a");if(file){SYSTEMTIME time;GetSystemTime(&time);std::fprintf(file,"\nSESSION pid=%lu utc=%04u-%02u-%02uT%02u:%02u:%02u display_mode=%d\n",GetCurrentProcessId(),time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond,display_mode);std::fclose(file);}
            AddVectoredExceptionHandler(0,exception_log);
            MH_CreateHook(reinterpret_cast<void*>(ClipCursor),reinterpret_cast<void*>(clip_cursor),reinterpret_cast<void**>(&original_clip));
            MH_CreateHook(reinterpret_cast<void*>(SetCursorPos),reinterpret_cast<void*>(warp_cursor),reinterpret_cast<void**>(&original_warp));
            MH_CreateHook(reinterpret_cast<void*>(MessageBoxA),reinterpret_cast<void*>(message_box),reinterpret_cast<void**>(&original_message_box));
            MH_CreateHook(reinterpret_cast<void*>(GetAsyncKeyState),reinterpret_cast<void*>(async_key),reinterpret_cast<void**>(&original_async_key));
            osm::start_backend_probe(directory);
            osm::start_movies(directory);
            osm::start_portable(directory,config_path);
            osm::start_engine_hooks(directory,config_path);
            osm::start_edition_hooks(config_path,log_path);
            osm::start_title_hooks(config_path,log_path);
            MH_EnableHook(MH_ALL_HOOKS);
        }
        global_hooks_ready=true;
    }
    wide=GetPrivateProfileIntA("Graphics","ExperimentalHorPlus",0,config_path)!=0;
    const int width=GetPrivateProfileIntA("Graphics","OutputWidth",1920,config_path);
    const int height=GetPrivateProfileIntA("Graphics","OutputHeight",1080,config_path);
    requested_width=width>=640&&width<=3840?width:1920;requested_height=height>=480&&height<=2160?height:1080;
    presentation_rect={0,0,static_cast<LONG>(requested_width),static_cast<LONG>(requested_height)};
    auto_window_aspect=GetPrivateProfileIntA("Graphics","AutoWindowAspect",1,config_path)!=0;
    if(width<640 || width>3840 || height<480 || height>2160)wide=false;
    scale=osm::horizontal_scale(static_cast<float>(width)/static_cast<float>(height>0?height:1));
    osm::engine_aspect=static_cast<float>(requested_width)/requested_height;
    char backend_path[MAX_PATH];std::snprintf(backend_path,MAX_PATH,"%sd3d8_backend.dll",directory);
    HMODULE backend=LoadLibraryExA(backend_path,nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);
    if(!backend) {
        GetSystemDirectoryA(backend_path,MAX_PATH);
        std::strcat(backend_path,"\\d3d8.dll");backend=LoadLibraryExA(backend_path,nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);
    }
    if(!backend)return nullptr;
    using Factory=IDirect3D8* (WINAPI *)(UINT);
    auto factory=reinterpret_cast<Factory>(GetProcAddress(backend,"Direct3DCreate8"));
    if(!factory)return nullptr;
    IDirect3D8* api=factory(sdk_version);
    if(api) {auto* table=const_cast<IDirect3D8Vtbl*>(api->lpVtbl);hook(&table->CreateDevice,create_device,real_create);}
    return api;
}

BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH) {
        self_module=module;DisableThreadLibraryCalls(module);
        char path[MAX_PATH],config[MAX_PATH];GetModuleFileNameA(module,path,MAX_PATH);
        char* slash=std::strrchr(path,'\\');
        if(slash){slash[1]=0;std::snprintf(config,MAX_PATH,"%sopensoccermania.ini",path);osm::prepare_portable(path,config);osm::early_portable_imports();}
    }
    return TRUE;
}
