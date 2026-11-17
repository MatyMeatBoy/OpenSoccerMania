// Asset-free worker interleaving regression; run on a private Win32 desktop.
#include <windows.h>
#include <d3d8.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <initializer_list>
static unsigned char image_padding[0x300000];
static unsigned yields=0,stops=0,enters=0,leaves=0;
static void check(bool ok,const char* text){if(!ok){std::fprintf(stderr,"FAIL: %s\n",text);std::exit(1);}}
static void __stdcall enter(void*){++enters;}
static void __stdcall leave(void*){++leaves;}
static void __stdcall yield(unsigned){++yields;if(!*reinterpret_cast<const BYTE*>(0x61ea9c))++stops;}
static LRESULT CALLBACK procedure(HWND w,UINT m,WPARAM a,LPARAM b){return DefWindowProcA(w,m,a,b);}
struct Code {
    BYTE bytes[128]={};unsigned size=0;
    void byte(BYTE b){bytes[size++]=b;}
    void word(DWORD w){std::memcpy(bytes+size,&w,4);size+=4;}
    void store(BYTE reg,DWORD address){byte(0x89);byte(reg);word(address);}
};
int main(int argc,char** argv){
    const bool unsupported=argc>1;
    const bool unverified=argc>1&&std::strcmp(argv[1],"--unverified")==0;
    if(unverified)check(WritePrivateProfileStringA("Graphics","VerifiedExecutable","0",".\\opensoccermania.ini")!=0,"Configure unverified executable");
    const auto begin=reinterpret_cast<std::uintptr_t>(image_padding),end=begin+sizeof(image_padding);
    check(GetModuleHandleA(nullptr)==reinterpret_cast<HMODULE>(0x400000)&&begin<=0x430400&&end>=0x61eac8,"Fixture addresses are unused image padding");
    const auto install=[](DWORD address,const void* data,unsigned size){DWORD old=0;check(VirtualProtect(reinterpret_cast<void*>(address),size,PAGE_EXECUTE_READWRITE,&old)!=0,"Executable fixture padding");std::memcpy(reinterpret_cast<void*>(address),data,size);FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(address),size);};
    // Snapshot/unlock and dispatch signatures match the verified worker. The
    // wrapper below models publication after the snapshot and before dispatch.
    BYTE context[]={0x8b,0x2d,0x0c,0x02,0x57,0x00,0x68,0x08,0xd6,0x61,0x00,0xff,0xd6,0x8b,0x1d,0x38,0xd6,0x61,0x00,0x68,0x08,0xd6,0x61,0x00,0xff,0xd7};
    if(argc>1&&std::strcmp(argv[1],"--unsupported")==0)context[0]=0x90;
    install(0x430463,context,sizeof(context));
    BYTE dispatch[]={0x8d,0x43,0xff,0x83,0xf8,0x16,0x0f,0x87,0x71,0x02,0x00,0x00,0xff,0x24,0x85,0x5c,0x07,0x43,0x00};
    if(argc>1&&std::strcmp(argv[1],"--unsupported-dispatch")==0)dispatch[17]=0x44;
    install(0x43047d,dispatch,sizeof(dispatch));
    BYTE completion[]={0x68,0x08,0xd6,0x61,0x00,0xff,0xd6,0x68,0x08,0xd6,0x61,0x00,0xc7,0x05,0x38,0xd6,0x61,0x00,0x00,0x00,0x00,0x00,0xff,0xd7,0x6a,0x00,0xff,0xd5};
    if(argc>1&&std::strcmp(argv[1],"--unsupported-completion")==0)completion[1]=0x0c;
    install(0x4306fa,completion,sizeof(completion));
    Code wrapper;for(BYTE b:{0x53,0x56,0x57,0x55})wrapper.byte(b);
    wrapper.store(0x25,0x61eac0); // initial ESP after saving callee-saved registers
    wrapper.byte(0x8b);wrapper.byte(0x1d);wrapper.word(0x61d638); // EBX=snapshot
    for(BYTE b:{0x8b,0x44,0x24,0x14})wrapper.byte(b); // late publication argument
    wrapper.byte(0xa3);wrapper.word(0x61d638);
    wrapper.byte(0xbe);wrapper.word(reinterpret_cast<DWORD>(enter));
    wrapper.byte(0xbf);wrapper.word(reinterpret_cast<DWORD>(leave));
    wrapper.byte(0xbd);wrapper.word(reinterpret_cast<DWORD>(yield));
    wrapper.byte(0xb8);wrapper.word(0x43047d);wrapper.byte(0xff);wrapper.byte(0xe0);
    install(0x430400,wrapper.bytes,wrapper.size);
    Code continuation;continuation.store(0x1d,0x61eab0);continuation.store(0x35,0x61eab4);continuation.store(0x3d,0x61eab8);continuation.store(0x2d,0x61eabc);continuation.store(0x25,0x61eac4);
    for(BYTE b:{0x5d,0x5f,0x5e,0x5b,0xc3})continuation.byte(b);
    install(0x430716,continuation.bytes,continuation.size);
    DWORD table[23];for(auto& target:table)target=0x430800;install(0x43075c,table,sizeof(table));
    Code handler;handler.byte(0xff);handler.byte(0x05);handler.word(0x61eaa8);
    handler.byte(0xc7);handler.byte(0x05);handler.word(0x61eaa0);handler.word(0x61d620);
    handler.byte(0xe9);handler.word(0x4306fa-(0x430800+handler.size+4));install(0x430800,handler.bytes,handler.size);
    WNDCLASSA cls={};cls.hInstance=GetModuleHandleA(nullptr);cls.lpfnWndProc=procedure;cls.lpszClassName="OSMSaveWorkerRegression";RegisterClassA(&cls);
    HWND window=CreateWindowA(cls.lpszClassName,"Private save worker regression",WS_OVERLAPPEDWINDOW,0,0,640,480,nullptr,nullptr,cls.hInstance,nullptr);
    HMODULE module=LoadLibraryA(".\\d3d8.dll");check(module!=nullptr,"Load production bridge");
    auto factory=reinterpret_cast<IDirect3D8* (WINAPI *)(UINT)>(GetProcAddress(module,"Direct3DCreate8"));check(factory!=nullptr,"Factory");
    IDirect3D8* api=factory(D3D_SDK_VERSION);check(api!=nullptr,"API");
    D3DPRESENT_PARAMETERS params={};params.BackBufferWidth=640;params.BackBufferHeight=480;params.BackBufferFormat=D3DFMT_X8R8G8B8;params.BackBufferCount=1;params.SwapEffect=D3DSWAPEFFECT_DISCARD;params.hDeviceWindow=window;params.Windowed=TRUE;
    IDirect3DDevice8* device=nullptr;check(SUCCEEDED(api->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,&params,&device)),"Device");
    auto& pending=*reinterpret_cast<DWORD*>(0x61d638);auto& output=*reinterpret_cast<DWORD*>(0x61eaa0);auto& processed=*reinterpret_cast<DWORD*>(0x61eaa8);auto& running=*reinterpret_cast<BYTE*>(0x61ea9c);
    const auto step=[&](DWORD queued){
        const DWORD snapshot=pending;reinterpret_cast<void (__cdecl *)(DWORD)>(0x430400)(queued);
        const auto* saved=reinterpret_cast<const DWORD*>(0x61eab0);
        check(saved[0]==snapshot&&saved[1]==reinterpret_cast<DWORD>(enter)&&saved[2]==reinterpret_cast<DWORD>(leave)&&saved[3]==reinterpret_cast<DWORD>(yield),"Guard preserves worker registers");
        check(*reinterpret_cast<const DWORD*>(0x61eac0)==*reinterpret_cast<const DWORD*>(0x61eac4),"Guard preserves worker stack");
    };
    pending=output=processed=0;running=1;step(4);
    if(unsupported){check(pending==0&&processed==0,"Unrecognized worker context remains untouched");}
    else{
        check(pending==4&&processed==0,"Idle snapshot must not erase a request published after that snapshot");
        step(4);check(pending==0&&processed==1&&output==0x61d620,"Retained request executes exactly once on the next pass");
        step(0);check(pending==0&&processed==1,"Idle pass must not repeat the completed request");
        for(DWORD command:{1u,23u}){pending=command;step(command);check(pending==0,"Valid command still completes");}
        check(processed==3,"Both valid dispatch boundaries execute");
        for(DWORD command:{24u,0xffffffffu}){pending=command;step(command);check(pending==0&&processed==3,"Invalid command handling remains unchanged");}
        running=0;pending=0;const auto before=yields;step(0);check(yields==before+1&&stops==1,"Idle guard reaches the worker's stopping/yield continuation");
    }
    check(enters==leaves,"Worker completion lock calls remain balanced");
    check(device->Release()==0,"Complete device release");api->Release();DestroyWindow(window);
    std::puts(unsupported?"PASS: unsupported worker code remains untouched":"PASS: idle worker preserves late requests, dispatch and context");
}
