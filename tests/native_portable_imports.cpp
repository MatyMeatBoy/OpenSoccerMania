// Exercise the real bridge's early IAT hooks, without touching machine registry.
#include <windows.h>
#include <shlobj.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
static unsigned forbidden_calls=0;
static LSTATUS WINAPI denied_create(HKEY,LPCSTR,DWORD,LPSTR,DWORD,REGSAM,const SECURITY_ATTRIBUTES*,PHKEY,LPDWORD){++forbidden_calls;return ERROR_ACCESS_DENIED;}
static BOOL WINAPI denied_folder(HWND,LPSTR,int,BOOL){++forbidden_calls;return FALSE;}
static void check(bool ok,const char* text){if(!ok){std::fprintf(stderr,"FAIL: %s\n",text);std::exit(1);}}
static void replace_import(const char* name,void* replacement){
    auto* base=reinterpret_cast<BYTE*>(GetModuleHandleA(nullptr));
    auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto* pe=reinterpret_cast<IMAGE_NT_HEADERS*>(base+dos->e_lfanew);
    auto* entry=reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base+pe->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    for(;entry->Name;++entry){
        if(!entry->OriginalFirstThunk)continue;
        auto* names=reinterpret_cast<IMAGE_THUNK_DATA*>(base+entry->OriginalFirstThunk);
        auto* slots=reinterpret_cast<IMAGE_THUNK_DATA*>(base+entry->FirstThunk);
        for(;names->u1.AddressOfData;++names,++slots){
            if(IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal))continue;
            auto* symbol=reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base+names->u1.AddressOfData);
            if(std::strcmp(reinterpret_cast<const char*>(symbol->Name),name))continue;
            DWORD protection=0;check(VirtualProtect(&slots->u1.Function,sizeof(void*),PAGE_READWRITE,&protection)!=FALSE,"Writable fixture IAT");
            slots->u1.Function=reinterpret_cast<ULONG_PTR>(replacement);
            VirtualProtect(&slots->u1.Function,sizeof(void*),protection,&protection);return;
        }
    }
    check(false,"Expected actual game API import present in fixture");
}
int main(){
    replace_import("RegCreateKeyExA",reinterpret_cast<void*>(denied_create));
    replace_import("SHGetSpecialFolderPathA",reinterpret_cast<void*>(denied_folder));
    HKEY key=nullptr;DWORD disposition=0;char folder[MAX_PATH]={};
    check(RegCreateKeyExA(HKEY_LOCAL_MACHINE,"Software\\LEGO Media\\Football Mania",0,nullptr,0,KEY_ALL_ACCESS,nullptr,&key,&disposition)==ERROR_ACCESS_DENIED,"Deny machine registry before installing bridge");
    check(!SHGetSpecialFolderPathA(nullptr,folder,0x1a,TRUE)&&forbidden_calls==2,"Deny original folder lookup before installing bridge");
    forbidden_calls=0;
    check(LoadLibraryA(".\\d3d8.dll")!=nullptr,"Load actual production bridge and early IAT hooks");
    check(RegCreateKeyExA(HKEY_LOCAL_MACHINE,"Software\\LEGO Media\\Football Mania",0,nullptr,0,KEY_ALL_ACCESS,nullptr,&key,&disposition)==ERROR_SUCCESS,"Actual imported RegCreateKeyExA is virtualized");
    check(key!=nullptr&&disposition==REG_OPENED_EXISTING_KEY,"Virtual key disposition");
    check(RegCloseKey(key)==ERROR_SUCCESS,"Actual imported RegCloseKey accepts virtual handle");
    check(SHGetSpecialFolderPathA(nullptr,folder,0x1a,TRUE)!=FALSE,"Actual imported SHGetSpecialFolderPathA is redirected");
    char expected[MAX_PATH]={};check(GetFullPathNameA(".\\profile",MAX_PATH,expected,nullptr)!=0,"Expected private profile");
    check(_stricmp(folder,expected)==0&&forbidden_calls==0,"Private profile with no original API calls");
    check(GetFileAttributesA(folder)!=INVALID_FILE_ATTRIBUTES,"Private profile directory exists");
    std::puts("PASS: actual game registry/create and special-folder imports stay private without machine access");
}
