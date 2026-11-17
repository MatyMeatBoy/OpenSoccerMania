// Asset-free reset regression; run only through tools/run_offscreen.py.
#include <windows.h>
#include <d3d8.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
static unsigned char image_padding[0x300000];
static void check(bool ok,const char* text){if(!ok){std::fprintf(stderr,"FAIL: %s\n",text);std::exit(1);}}
static LRESULT CALLBACK procedure(HWND w,UINT m,WPARAM a,LPARAM b){return DefWindowProcA(w,m,a,b);}
int main(int argc,char** argv){
    const bool unsupported=argc>1&&std::strcmp(argv[1],"--unsupported")==0;
    const bool trace=argc>1&&std::strcmp(argv[1],"--trace")==0;
    check(WritePrivateProfileStringA("Diagnostics","TraceSaveStatus",trace?"1":"0",".\\opensoccermania.ini")!=0,"Configure diagnostic trace variant");
    const auto begin=reinterpret_cast<std::uintptr_t>(image_padding),end=begin+sizeof(image_padding);
    check(GetModuleHandleA(nullptr)==reinterpret_cast<HMODULE>(0x400000)&&begin<=0x430a20&&end>=0x683754,"Fixture addresses are unused image padding");
    auto install=[](std::uintptr_t address,const BYTE* data,size_t bytes){DWORD old=0;check(VirtualProtect(reinterpret_cast<void*>(address),bytes,PAGE_EXECUTE_READWRITE,&old)!=0,"Executable fixture memory");std::memcpy(reinterpret_cast<void*>(address),data,bytes);FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(address),bytes);};
    const BYTE accessor[]={0xa1,0xa4,0xea,0x61,0x00,0xc3};install(0x430a20,accessor,sizeof(accessor));
    BYTE caller[]={0xe8,0xe5,0x6d,0xf3,0xff,0xb9,0x07,0x00,0x00,0x00,0xc3};if(unsupported)caller[5]=0xba;install(0x4f9c36,caller,sizeof(caller));
    const BYTE consumer[]={0x8b,0x3d,0x50,0x37,0x68,0x00};install(0x4f9cc9,consumer,sizeof(consumer));
    WNDCLASSA cls={};cls.hInstance=GetModuleHandleA(nullptr);cls.lpfnWndProc=procedure;cls.lpszClassName="OSMResetRegression";RegisterClassA(&cls);
    HWND window=CreateWindowA(cls.lpszClassName,"Private reset regression",WS_OVERLAPPEDWINDOW,0,0,640,480,nullptr,nullptr,cls.hInstance,nullptr);
    HMODULE module=LoadLibraryA(".\\d3d8.dll");check(module!=nullptr,"Load production bridge");
    auto factory=reinterpret_cast<IDirect3D8* (WINAPI *)(UINT)>(GetProcAddress(module,"Direct3DCreate8"));check(factory!=nullptr,"Factory");
    IDirect3D8* api=factory(D3D_SDK_VERSION);check(api!=nullptr,"API");
    D3DPRESENT_PARAMETERS params={};params.BackBufferWidth=640;params.BackBufferHeight=480;params.BackBufferFormat=D3DFMT_X8R8G8B8;params.BackBufferCount=1;params.SwapEffect=D3DSWAPEFFECT_DISCARD;params.hDeviceWindow=window;params.Windowed=TRUE;
    IDirect3DDevice8* device=nullptr;check(SUCCEEDED(api->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,&params,&device)),"Device");

    using Status=int (__cdecl *)();
    const auto status=reinterpret_cast<Status>(0x430a20),metadata_status=reinterpret_cast<Status>(0x4f9c36);
    auto*& header=*reinterpret_cast<const char**>(0x683750);auto& result=*reinterpret_cast<unsigned*>(0x61eaa4);
    if(unsupported){result=0;header=nullptr;check(metadata_status()==0&&header==nullptr,"Unknown caller code must not activate the metadata hook");device->Release();api->Release();DestroyWindow(window);std::puts("PASS: unsupported save metadata code remains untouched");return 0;}
    result=0;header=nullptr;SetLastError(0x51a7);check(metadata_status()==0,"Status preserved");check(GetLastError()==0x51a7,"Metadata guard must preserve the caller's Win32 error state");check(header&&header[0]==0,"Successful null metadata must become a safe empty record");
    char tab[]="\t";header=tab;metadata_status();check(header&&header[0]==0,"Blank placeholder metadata must not enter the legacy control-character scan");check(tab[0]=='\t',"Original metadata bytes remain unchanged");
    const char valid[]="LEGO\tSTORY TEAM\tBRONZE";header=valid;metadata_status();check(header==valid,"Valid metadata pointer and bytes remain unchanged");
    char no_team[]="LEGO\t";header=no_team;metadata_status();check(header&&std::strcmp(header,"LEGO")==0,"Missing team field must stop at NUL rather than scan beyond the record");
    metadata_status();check(header&&std::strcmp(header,"LEGO")==0,"Repeated polling preserves a previously normalized record");
    char unterminated[64];std::memset(unterminated,'A',sizeof(unterminated));header=unterminated;metadata_status();check(header&&header[0]==0,"Unterminated metadata cannot overflow the legacy stack buffer");
    auto* inaccessible=static_cast<char*>(VirtualAlloc(nullptr,8192,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));check(inaccessible!=nullptr,"Guard fixture allocation");DWORD old=0;check(VirtualProtect(inaccessible+4096,4096,PAGE_NOACCESS,&old)!=0,"Guard fixture protection");
    std::memset(inaccessible+4090,'A',6);header=inaccessible+4090;metadata_status();check(header&&header[0]==0,"Unterminated record must not read into an inaccessible page");header=inaccessible+4096;metadata_status();check(header&&header[0]==0,"Unreadable metadata becomes an empty record without dereference");VirtualFree(inaccessible,0,MEM_RELEASE);
    result=2;header=nullptr;SetLastError(0x51a8);check(metadata_status()==2&&header==nullptr,"Failed result is unchanged");check(GetLastError()==0x51a8,"Failed status preserves Win32 error state");
    result=0;header=nullptr;SetLastError(0x51a9);check(status()==0&&header==nullptr,"Other callers retain the original status behavior");check(GetLastError()==0x51a9,"Other callers preserve Win32 error state");
    device->Release();api->Release();DestroyWindow(window);std::puts("PASS: production save metadata guard preserves valid records and contains blank/invalid results");
}
