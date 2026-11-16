#define CINTERFACE
#define COBJMACROS
#include <windows.h>
#include <cstdio>
#include <d3d11.h>
#include <dxgi1_2.h>
#include "../third_party/minhook/include/MinHook.h"

namespace osm {
using DeviceFactory=HRESULT (WINAPI *)(IDXGIAdapter*,D3D_DRIVER_TYPE,HMODULE,UINT,const D3D_FEATURE_LEVEL*,UINT,UINT,ID3D11Device**,D3D_FEATURE_LEVEL*,ID3D11DeviceContext**);
using SwapFactory=HRESULT (WINAPI *)(IDXGIAdapter*,D3D_DRIVER_TYPE,HMODULE,UINT,const D3D_FEATURE_LEVEL*,UINT,UINT,const DXGI_SWAP_CHAIN_DESC*,IDXGISwapChain**,ID3D11Device**,D3D_FEATURE_LEVEL*,ID3D11DeviceContext**);
using TextureFactory=HRESULT (WINAPI *)(ID3D11Device*,const D3D11_TEXTURE2D_DESC*,const D3D11_SUBRESOURCE_DATA*,ID3D11Texture2D**);
using ChainFactory=HRESULT (WINAPI *)(IDXGIFactory*,IUnknown*,DXGI_SWAP_CHAIN_DESC*,IDXGISwapChain**);
using ChainFactory2=HRESULT (WINAPI *)(IDXGIFactory2*,IUnknown*,HWND,const DXGI_SWAP_CHAIN_DESC1*,const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*,IDXGIOutput*,IDXGISwapChain1**);
using ChainPresent=HRESULT (WINAPI *)(IDXGISwapChain*,UINT,UINT);
static DeviceFactory original_device;
static SwapFactory original_swap;
static TextureFactory original_texture;
static ChainFactory original_chain;
static ChainFactory2 original_chain2;
static ChainPresent original_present;
static bool vsync_requested=true;
static bool offscreen_probe=false;
static char probe_config[MAX_PATH];
static HANDLE probe_timer=nullptr;
static LARGE_INTEGER probe_previous={};
static unsigned probe_fps=60,probe_frames=0;
static LARGE_INTEGER frequency={};
static char probe_path[MAX_PATH];
static unsigned render_width=0,render_height=0;

template<class T> static void replace(T* slot,T replacement,T& original) {
    if(*slot==replacement)return;
    DWORD protection;if(!VirtualProtect(slot,sizeof(T),PAGE_READWRITE,&protection))return;
    original=*slot;*slot=replacement;VirtualProtect(slot,sizeof(T),protection,&protection);
}
static HRESULT WINAPI present_chain(IDXGISwapChain* swap,UINT interval,UINT flags) {
    if(offscreen_probe&&!(flags&DXGI_PRESENT_TEST)) {
        if(!(probe_frames++%60)){const unsigned value=GetPrivateProfileIntA("Diagnostics","ProbeFPS",60,probe_config);probe_fps=value>=25&&value<=480?value:60;}
        LARGE_INTEGER now;QueryPerformanceCounter(&now);
        if(probe_previous.QuadPart&&frequency.QuadPart) {
            const double remaining=1./probe_fps-static_cast<double>(now.QuadPart-probe_previous.QuadPart)/frequency.QuadPart;
            if(remaining>0) {
                if(!probe_timer)probe_timer=CreateWaitableTimerExW(nullptr,nullptr,0x2,TIMER_ALL_ACCESS);
                if(probe_timer){LARGE_INTEGER due;due.QuadPart=-static_cast<LONGLONG>(remaining*10000000);if(SetWaitableTimer(probe_timer,&due,0,nullptr,nullptr,FALSE))WaitForSingleObject(probe_timer,1000);}
                else Sleep(static_cast<DWORD>(remaining*1000));
            }
        }
        QueryPerformanceCounter(&probe_previous);
    }
    const UINT effective=vsync_requested||(flags&DXGI_PRESENT_TEST)?interval:0;
    LARGE_INTEGER start,end;QueryPerformanceCounter(&start);
    const HRESULT result=original_present(swap,effective,flags);
    QueryPerformanceCounter(&end);
    static unsigned frames=0,zero=0,nonzero=0;
    static LONGLONG ticks=0;
    ticks+=end.QuadPart-start.QuadPart;if(effective)++nonzero;else ++zero;
    if(++frames==300){FILE* file=std::fopen(probe_path,"a");if(file){std::fprintf(file,"DXGI_PRESENT samples=%u sync0=%u synced=%u avg_ms=%.4f vsync_requested=%d last_result=0x%lx\n",frames,zero,nonzero,1000.*ticks/frequency.QuadPart/frames,vsync_requested,result);std::fclose(file);}frames=zero=nonzero=0;ticks=0;}
    return offscreen_probe&&result==DXGI_STATUS_OCCLUDED?S_OK:result;
}
static void observe_chain(IDXGISwapChain* swap) {
    if(!swap)return;
    auto* table=const_cast<IDXGISwapChainVtbl*>(swap->lpVtbl);
    if(table->Present==present_chain)return;
    replace(&table->Present,present_chain,original_present);
    DXGI_SWAP_CHAIN_DESC desc={};
    if(SUCCEEDED(IDXGISwapChain_GetDesc(swap,&desc))){FILE* file=std::fopen(probe_path,"a");if(file){std::fprintf(file,"DXGI_CHAIN size=%ux%u buffers=%u effect=%u flags=0x%x windowed=%d samples=%u\n",desc.BufferDesc.Width,desc.BufferDesc.Height,desc.BufferCount,desc.SwapEffect,desc.Flags,desc.Windowed,desc.SampleDesc.Count);std::fclose(file);}}
}
static HRESULT WINAPI create_chain(IDXGIFactory* factory,IUnknown* device,DXGI_SWAP_CHAIN_DESC* desc,IDXGISwapChain** output) {
    const HRESULT result=original_chain(factory,device,desc,output);if(SUCCEEDED(result)&&output)observe_chain(*output);return result;
}
static HRESULT WINAPI create_chain2(IDXGIFactory2* factory,IUnknown* device,HWND window,const DXGI_SWAP_CHAIN_DESC1* desc,const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* full,IDXGIOutput* restrict_output,IDXGISwapChain1** output) {
    const HRESULT result=original_chain2(factory,device,window,desc,full,restrict_output,output);
    if(SUCCEEDED(result)&&output)observe_chain(reinterpret_cast<IDXGISwapChain*>(*output));return result;
}
static void observe_factory(ID3D11Device* device) {
    IDXGIDevice* dxgi=nullptr;
    if(FAILED(ID3D11Device_QueryInterface(device,IID_IDXGIDevice,reinterpret_cast<void**>(&dxgi))))return;
    IDXGIAdapter* adapter=nullptr;
    if(SUCCEEDED(IDXGIDevice_GetAdapter(dxgi,&adapter))) {
        DXGI_ADAPTER_DESC desc={};if(SUCCEEDED(IDXGIAdapter_GetDesc(adapter,&desc))){FILE* file=std::fopen(probe_path,"a");if(file){std::fprintf(file,"GPU name=%ls dedicated_mib=%llu\n",desc.Description,static_cast<unsigned long long>(desc.DedicatedVideoMemory)/1048576);std::fclose(file);}}
        IDXGIFactory* factory=nullptr;
        if(SUCCEEDED(IDXGIAdapter_GetParent(adapter,IID_IDXGIFactory,reinterpret_cast<void**>(&factory)))) {
            auto* table=const_cast<IDXGIFactoryVtbl*>(factory->lpVtbl);replace(&table->CreateSwapChain,create_chain,original_chain);
            IDXGIFactory2* extended=nullptr;
            if(SUCCEEDED(IDXGIFactory_QueryInterface(factory,IID_IDXGIFactory2,reinterpret_cast<void**>(&extended)))) {
                auto* extra=const_cast<IDXGIFactory2Vtbl*>(extended->lpVtbl);replace(&extra->CreateSwapChainForHwnd,create_chain2,original_chain2);IDXGIFactory2_Release(extended);
            }
            IDXGIFactory_Release(factory);
        }
        IDXGIAdapter_Release(adapter);
    }
    IDXGIDevice_Release(dxgi);
}

static HRESULT WINAPI texture(ID3D11Device* device,const D3D11_TEXTURE2D_DESC* desc,const D3D11_SUBRESOURCE_DATA* data,ID3D11Texture2D** result) {
    HRESULT status=original_texture(device,desc,data,result);
    if(SUCCEEDED(status)&&desc&&(desc->BindFlags&(D3D11_BIND_RENDER_TARGET|D3D11_BIND_DEPTH_STENCIL))) {
        if(desc->Width*desc->Height>render_width*render_height){render_width=desc->Width;render_height=desc->Height;}
        FILE* file=std::fopen(probe_path,"a");
        if(file){std::fprintf(file,"target=%ux%u samples=%u format=%u bind=0x%x\n",desc->Width,desc->Height,desc->SampleDesc.Count,desc->Format,desc->BindFlags);std::fclose(file);}
    }
    return status;
}

static void observe(ID3D11Device* device) {
    if(!device)return;
    observe_factory(device);
    auto* table=const_cast<ID3D11DeviceVtbl*>(device->lpVtbl);
    if(table->CreateTexture2D==texture)return;
    DWORD protect;
    if(VirtualProtect(&table->CreateTexture2D,sizeof(void*),PAGE_READWRITE,&protect)) {
        original_texture=table->CreateTexture2D;table->CreateTexture2D=texture;
        VirtualProtect(&table->CreateTexture2D,sizeof(void*),protect,&protect);
    }
}

static HRESULT WINAPI device_factory(IDXGIAdapter* adapter,D3D_DRIVER_TYPE type,HMODULE software,UINT flags,const D3D_FEATURE_LEVEL* levels,UINT count,UINT sdk,ID3D11Device** device,D3D_FEATURE_LEVEL* level,ID3D11DeviceContext** context) {
    HRESULT status=original_device(adapter,type,software,flags,levels,count,sdk,device,level,context);
    if(SUCCEEDED(status)&&device)observe(*device);return status;
}
static HRESULT WINAPI swap_factory(IDXGIAdapter* adapter,D3D_DRIVER_TYPE type,HMODULE software,UINT flags,const D3D_FEATURE_LEVEL* levels,UINT count,UINT sdk,const DXGI_SWAP_CHAIN_DESC* desc,IDXGISwapChain** swap,ID3D11Device** device,D3D_FEATURE_LEVEL* level,ID3D11DeviceContext** context) {
    HRESULT status=original_swap(adapter,type,software,flags,levels,count,sdk,desc,swap,device,level,context);
    if(SUCCEEDED(status)){if(device)observe(*device);if(swap)observe_chain(*swap);}return status;
}
void start_backend_probe(const char* directory) {
    std::snprintf(probe_path,MAX_PATH,"%sopensoccermania-backend.log",directory);
    std::snprintf(probe_config,MAX_PATH,"%sopensoccermania.ini",directory);
    vsync_requested=GetPrivateProfileIntA("Graphics","VSync",1,probe_config)!=0;
    offscreen_probe=GetPrivateProfileIntA("Diagnostics","IsolateInstance",0,probe_config)!=0;
    QueryPerformanceFrequency(&frequency);
    HMODULE library=LoadLibraryA("d3d11.dll");if(!library)return;
    MH_CreateHook(reinterpret_cast<void*>(GetProcAddress(library,"D3D11CreateDevice")),reinterpret_cast<void*>(device_factory),reinterpret_cast<void**>(&original_device));
    MH_CreateHook(reinterpret_cast<void*>(GetProcAddress(library,"D3D11CreateDeviceAndSwapChain")),reinterpret_cast<void*>(swap_factory),reinterpret_cast<void**>(&original_swap));
}
}
