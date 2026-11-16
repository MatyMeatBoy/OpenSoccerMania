// Asset-free integration test. Run only through tools/run_offscreen.py.
#include <windows.h>
#include <d3d8.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
static void check(bool value,const char* text){if(!value){std::fprintf(stderr,"FAIL: %s\n",text);std::exit(1);}}
static LRESULT CALLBACK procedure(HWND window,UINT message,WPARAM w,LPARAM l){return DefWindowProcA(window,message,w,l);}
int main() {
    WritePrivateProfileStringA("Graphics","CameraZoom","0",".\\opensoccermania.ini");
    char name[128];std::snprintf(name,sizeof(name),"Local\\OpenSoccerMania-Input-%lu",GetCurrentProcessId());
    HANDLE mapping=CreateFileMappingA(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,272,name);
    auto* input=static_cast<BYTE*>(MapViewOfFile(mapping,FILE_MAP_WRITE,0,0,272));check(input!=nullptr,"Create private test input");
    auto* words=reinterpret_cast<DWORD*>(input);std::memset(input,0,272);words[0]=0x494d534f;words[2]=3;
    auto keys=[&](UINT key){InterlockedIncrement(reinterpret_cast<volatile LONG*>(words+1));std::memset(input+16,0,256);if(key){UINT scan=MapVirtualKeyW(key,MAPVK_VK_TO_VSC);if(key==VK_RIGHT)scan|=0x80;input[16+scan]=0x80;}MemoryBarrier();InterlockedIncrement(reinterpret_cast<volatile LONG*>(words+1));};
    WNDCLASSA cls={};cls.hInstance=GetModuleHandleA(nullptr);cls.lpfnWndProc=procedure;cls.lpszClassName="OSMSettingsRegression";RegisterClassA(&cls);
    HWND window=CreateWindowA(cls.lpszClassName,"OpenSoccerMania settings test",WS_OVERLAPPEDWINDOW,0,0,640,480,nullptr,nullptr,cls.hInstance,nullptr);
    HMODULE module=LoadLibraryA(".\\d3d8.dll");check(module!=nullptr,"Load bridge");
    auto factory=reinterpret_cast<IDirect3D8* (WINAPI *)(UINT)>(GetProcAddress(module,"Direct3DCreate8"));check(factory!=nullptr,"D3D8 factory");
    IDirect3D8* api=factory(D3D_SDK_VERSION);check(api!=nullptr,"Create API");
    D3DPRESENT_PARAMETERS params={};params.BackBufferWidth=640;params.BackBufferHeight=480;params.BackBufferFormat=D3DFMT_X8R8G8B8;params.BackBufferCount=1;params.SwapEffect=D3DSWAPEFFECT_DISCARD;params.hDeviceWindow=window;params.Windowed=TRUE;
    IDirect3DDevice8* device=nullptr;check(SUCCEEDED(api->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,&params,&device)),"Create device");
    std::snprintf(name,sizeof(name),"Local\\OpenSoccerMania-UI-%lu",GetCurrentProcessId());
    HANDLE ui_map=OpenFileMappingA(FILE_MAP_READ,FALSE,name);check(ui_map!=nullptr,"Native UI publisher exists");
    auto* ui=static_cast<const volatile DWORD*>(MapViewOfFile(ui_map,FILE_MAP_READ,0,0,32));check(ui&&ui[0]==0x554d534f&&ui[1]==1,"UI protocol");
    keys(VK_F1);
    device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000,1,0);check(SUCCEEDED(device->Present(nullptr,nullptr,nullptr,nullptr)),"Open panel present");
    check(ui[2]&1,"F1 opened native panel through private input");
    keys(0);device->Present(nullptr,nullptr,nullptr,nullptr);
    keys(VK_RIGHT);device->Present(nullptr,nullptr,nullptr,nullptr);
    check(GetPrivateProfileIntA("Graphics","CameraZoom",0,".\\opensoccermania.ini")==1,"Panel persisted camera preset");
    keys(0);
    FILE* request=std::fopen("opensoccermania-capture.request","wb");check(request!=nullptr,"Request owned backbuffer capture");std::fclose(request);
    for(int frame=3;frame<60;++frame){device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000,1,0);check(SUCCEEDED(device->Present(nullptr,nullptr,nullptr,nullptr)),"Panel rendered");}
    FILE* image=std::fopen("osm-frame-60.bmp","rb");check(image!=nullptr,"Captured panel frame");
    BITMAPFILEHEADER header={};BITMAPINFOHEADER info={};std::fread(&header,sizeof(header),1,image);std::fread(&info,sizeof(info),1,image);
    check(info.biWidth==1920&&std::abs(info.biHeight)==1080,"Physical panel output dimensions");
    const long row=info.biHeight<0?540:info.biHeight-1-540;
    const long offset=header.bfOffBits+row*info.biWidth*4+960*4;std::fseek(image,offset,SEEK_SET);DWORD pixel=0;std::fread(&pixel,4,1,image);std::fclose(image);
    check((pixel&0xffffff)!=0,"Panel center reached GPU output");
    keys(VK_F1);device->Present(nullptr,nullptr,nullptr,nullptr);check(!(ui[2]&1),"F1 closed panel and released input");keys(0);
    device->Release();api->Release();DestroyWindow(window);UnmapViewOfFile(const_cast<const DWORD*>(ui));CloseHandle(ui_map);UnmapViewOfFile(input);CloseHandle(mapping);
    std::puts("PASS: native F1 panel, persistence, GPU output and input release");
}
