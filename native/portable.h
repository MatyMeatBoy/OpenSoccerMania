#pragma once
#include <string>
#include <vector>
#include <cstdlib>
#include "edition.h"

namespace osm {
static char profile_path[MAX_PATH],legacy_ini[MAX_PATH],portable_config[MAX_PATH],game_directory[MAX_PATH];
static HKEY virtual_key=reinterpret_cast<HKEY>(static_cast<ULONG_PTR>(0xf053504f));
using OpenRegistry=LSTATUS (WINAPI *)(HKEY,LPCSTR,DWORD,REGSAM,PHKEY);
using OpenRegistryOld=LSTATUS (WINAPI *)(HKEY,LPCSTR,PHKEY);
using CreateRegistry=LSTATUS (WINAPI *)(HKEY,LPCSTR,DWORD,LPSTR,DWORD,REGSAM,const SECURITY_ATTRIBUTES*,PHKEY,LPDWORD);
using QueryRegistry=LSTATUS (WINAPI *)(HKEY,LPCSTR,LPDWORD,LPDWORD,LPBYTE,LPDWORD);
using SetRegistry=LSTATUS (WINAPI *)(HKEY,LPCSTR,DWORD,DWORD,const BYTE*,DWORD);
using CloseRegistry=LSTATUS (WINAPI *)(HKEY);
using FolderPath=HRESULT (WINAPI *)(HWND,int,HANDLE,DWORD,LPSTR);
using SpecialFolderPath=BOOL (WINAPI *)(HWND,LPSTR,int,BOOL);
static OpenRegistry original_open_registry;
static OpenRegistryOld original_open_registry_old;
static CreateRegistry original_create_registry;
static QueryRegistry original_query_registry;
static SetRegistry original_set_registry;
static CloseRegistry original_close_registry;
static FolderPath original_folder_path;
static SpecialFolderPath original_special_folder_path;
using CreateMutex=HANDLE (WINAPI *)(LPSECURITY_ATTRIBUTES,BOOL,LPCSTR);
static CreateMutex original_create_mutex;
static HANDLE WINAPI create_mutex(LPSECURITY_ATTRIBUTES security,BOOL owned,LPCSTR name) {
    if(name&&(_stricmp(name,"FootballMania")==0||_stricmp(name,"SoccerMania")==0||_stricmp(name,"Global\\SDREAMS_MUTEX")==0)&&GetPrivateProfileIntA("Diagnostics","IsolateInstance",0,portable_config)) {
        char isolated[96];std::snprintf(isolated,sizeof(isolated),"OpenSoccerMania-Probe-%lu-%s",GetCurrentProcessId(),name);
        for(char* item=isolated;*item;++item)if(*item=='\\'||*item=='/')*item='_';
        return original_create_mutex(security,owned,isolated);
    }
    return original_create_mutex(security,owned,name);
}

static bool legacy_name(LPCSTR name) {
    if(!name)return false;
    return _strnicmp(name,"Software\\LEGO Media\\Football Mania",34)==0 ||
           _strnicmp(name,"Software\\LEGO Media\\Soccer Mania",32)==0;
}
static bool selected_legacy_name(LPCSTR name) {
    const bool american=configured_edition(portable_config)==Edition::Soccer;
    return american?std::strstr(name,"Soccer Mania")!=nullptr:std::strstr(name,"Football Mania")!=nullptr;
}
static LSTATUS WINAPI open_registry(HKEY key,LPCSTR name,DWORD options,REGSAM access,PHKEY output) {
    if(legacy_name(name)){if(!selected_legacy_name(name))return ERROR_FILE_NOT_FOUND;if(!output)return ERROR_INVALID_PARAMETER;*output=virtual_key;return ERROR_SUCCESS;}
    return original_open_registry(key,name,options,access,output);
}
static LSTATUS WINAPI open_registry_old(HKEY key,LPCSTR name,PHKEY output) {
    if(legacy_name(name)){if(!selected_legacy_name(name))return ERROR_FILE_NOT_FOUND;if(!output)return ERROR_INVALID_PARAMETER;*output=virtual_key;return ERROR_SUCCESS;}
    return original_open_registry_old(key,name,output);
}
static LSTATUS WINAPI create_registry(HKEY key,LPCSTR name,DWORD reserved,LPSTR cls,DWORD options,REGSAM access,const SECURITY_ATTRIBUTES* security,PHKEY output,LPDWORD disposition) {
    if(legacy_name(name)){
        if(!selected_legacy_name(name))return ERROR_FILE_NOT_FOUND;
        if(!output)return ERROR_INVALID_PARAMETER;
        *output=virtual_key;
        if(disposition)*disposition=REG_OPENED_EXISTING_KEY;
        return ERROR_SUCCESS;
    }
    return original_create_registry(key,name,reserved,cls,options,access,security,output,disposition);
}
static LSTATUS WINAPI query_registry(HKEY key,LPCSTR name,LPDWORD reserved,LPDWORD type,LPBYTE data,LPDWORD length) {
    if(key!=virtual_key)return original_query_registry(key,name,reserved,type,data,length);
    if(!name||!length)return ERROR_INVALID_PARAMETER;
    std::vector<BYTE> bytes;DWORD kind=REG_SZ;
    if(_stricmp(name,"Location")==0)bytes.assign(game_directory,game_directory+std::strlen(game_directory)+1);
    if(_stricmp(name,"Language")==0) {
        char language[32];GetPrivateProfileStringA("Game","Language","AUTO",language,sizeof(language),portable_config);
        if(const char* region=edition_registry_language(portable_config))std::snprintf(language,sizeof(language),"%s",region);
        if(_stricmp(language,"AUTO")!=0)bytes.assign(language,language+std::strlen(language)+1);
    }
    if(bytes.empty()) {
        char encoded[32768];GetPrivateProfileStringA("Legacy",name,"",encoded,sizeof(encoded),legacy_ini);
        char* colon=std::strchr(encoded,':');
        if(!colon)return ERROR_FILE_NOT_FOUND;
        *colon=0;kind=static_cast<DWORD>(std::strtoul(encoded,nullptr,10));
        const char* hex=colon+1;
        for(size_t i=0;hex[i]&&hex[i+1];i+=2){char pair[3]={hex[i],hex[i+1],0};bytes.push_back(static_cast<BYTE>(std::strtoul(pair,nullptr,16)));}
    }
    const DWORD required=static_cast<DWORD>(bytes.size());
    if(type)*type=kind;
    if(!data){*length=required;return ERROR_SUCCESS;}
    if(*length<required){*length=required;return ERROR_MORE_DATA;}
    std::memcpy(data,bytes.data(),required);*length=required;return ERROR_SUCCESS;
}
static LSTATUS WINAPI set_registry(HKEY key,LPCSTR name,DWORD reserved,DWORD type,const BYTE* data,DWORD length) {
    if(key!=virtual_key)return original_set_registry(key,name,reserved,type,data,length);
    if(!name||(!data&&length)||length>16000)return ERROR_INVALID_PARAMETER;
    std::string value=std::to_string(type)+":";value.reserve(length*2+16);
    constexpr char hex[]="0123456789abcdef";
    for(DWORD i=0;i<length;++i){value+=hex[data[i]>>4];value+=hex[data[i]&15];}
    return WritePrivateProfileStringA("Legacy",name,value.c_str(),legacy_ini)?ERROR_SUCCESS:ERROR_WRITE_FAULT;
}
static LSTATUS WINAPI close_registry(HKEY key) {return key==virtual_key?ERROR_SUCCESS:original_close_registry(key);}
static HRESULT WINAPI folder_path(HWND window,int folder,HANDLE token,DWORD flags,LPSTR output) {
    if(output&&((folder&255)==0x1a||(folder&255)==0x1c)){std::strcpy(output,profile_path);return S_OK;}
    return original_folder_path(window,folder,token,flags,output);
}
static BOOL WINAPI special_folder_path(HWND window,LPSTR output,int folder,BOOL create) {
    if((folder&255)==0x1a||(folder&255)==0x1c){
        if(!output)return FALSE;
        std::strcpy(output,profile_path);return TRUE;
    }
    return original_special_folder_path(window,output,folder,create);
}
static void prepare_portable(const char* directory,const char* config) {
    std::snprintf(game_directory,MAX_PATH,"%s",directory);
    std::snprintf(profile_path,MAX_PATH,"%sprofile",directory);CreateDirectoryA(profile_path,nullptr);
    std::snprintf(legacy_ini,MAX_PATH,"%s\\legacy.ini",profile_path);
    std::snprintf(portable_config,MAX_PATH,"%s",config);
}
static void early_portable_imports() {
    auto base=reinterpret_cast<unsigned char*>(GetModuleHandleA(nullptr));
    auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE)return;
    auto headers=reinterpret_cast<IMAGE_NT_HEADERS*>(base+dos->e_lfanew);
    auto rva=headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if(!rva)return;
    auto descriptor=reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base+rva);
    for(;descriptor->Name;++descriptor) {
        if(!descriptor->OriginalFirstThunk)continue;
        auto names=reinterpret_cast<IMAGE_THUNK_DATA*>(base+descriptor->OriginalFirstThunk);
        auto slots=reinterpret_cast<IMAGE_THUNK_DATA*>(base+descriptor->FirstThunk);
        for(;names->u1.AddressOfData;++names,++slots) {
            if(IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal))continue;
            const char* name=reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base+names->u1.AddressOfData)->Name;
            void* replacement=nullptr;void** original=nullptr;
            if(std::strcmp(name,"RegOpenKeyExA")==0){replacement=reinterpret_cast<void*>(open_registry);original=reinterpret_cast<void**>(&original_open_registry);}
            if(std::strcmp(name,"RegOpenKeyA")==0){replacement=reinterpret_cast<void*>(open_registry_old);original=reinterpret_cast<void**>(&original_open_registry_old);}
            if(std::strcmp(name,"RegCreateKeyExA")==0){replacement=reinterpret_cast<void*>(create_registry);original=reinterpret_cast<void**>(&original_create_registry);}
            if(std::strcmp(name,"RegQueryValueExA")==0){replacement=reinterpret_cast<void*>(query_registry);original=reinterpret_cast<void**>(&original_query_registry);}
            if(std::strcmp(name,"RegSetValueExA")==0){replacement=reinterpret_cast<void*>(set_registry);original=reinterpret_cast<void**>(&original_set_registry);}
            if(std::strcmp(name,"RegCloseKey")==0){replacement=reinterpret_cast<void*>(close_registry);original=reinterpret_cast<void**>(&original_close_registry);}
            if(std::strcmp(name,"SHGetFolderPathA")==0){replacement=reinterpret_cast<void*>(folder_path);original=reinterpret_cast<void**>(&original_folder_path);}
            if(std::strcmp(name,"SHGetSpecialFolderPathA")==0){replacement=reinterpret_cast<void*>(special_folder_path);original=reinterpret_cast<void**>(&original_special_folder_path);}
            if(std::strcmp(name,"CreateMutexA")==0){replacement=reinterpret_cast<void*>(create_mutex);original=reinterpret_cast<void**>(&original_create_mutex);}
            if(replacement) {
                DWORD protect;auto slot=reinterpret_cast<void**>(&slots->u1.Function);
                if(VirtualProtect(slot,sizeof(void*),PAGE_READWRITE,&protect)) {
                    *original=*slot;*slot=replacement;VirtualProtect(slot,sizeof(void*),protect,&protect);
                }
            }
        }
    }
}
static void start_portable(const char* directory,const char* config) {
    prepare_portable(directory,config);
    MH_CreateHook(reinterpret_cast<void*>(RegOpenKeyExA),reinterpret_cast<void*>(open_registry),reinterpret_cast<void**>(&original_open_registry));
    MH_CreateHook(reinterpret_cast<void*>(RegOpenKeyA),reinterpret_cast<void*>(open_registry_old),reinterpret_cast<void**>(&original_open_registry_old));
    MH_CreateHook(reinterpret_cast<void*>(RegCreateKeyExA),reinterpret_cast<void*>(create_registry),reinterpret_cast<void**>(&original_create_registry));
    MH_CreateHook(reinterpret_cast<void*>(RegQueryValueExA),reinterpret_cast<void*>(query_registry),reinterpret_cast<void**>(&original_query_registry));
    MH_CreateHook(reinterpret_cast<void*>(RegSetValueExA),reinterpret_cast<void*>(set_registry),reinterpret_cast<void**>(&original_set_registry));
    MH_CreateHook(reinterpret_cast<void*>(RegCloseKey),reinterpret_cast<void*>(close_registry),reinterpret_cast<void**>(&original_close_registry));
    HMODULE shell=LoadLibraryA("shell32.dll");
    if(shell)MH_CreateHook(reinterpret_cast<void*>(GetProcAddress(shell,"SHGetFolderPathA")),reinterpret_cast<void*>(folder_path),reinterpret_cast<void**>(&original_folder_path));
    if(shell)MH_CreateHook(reinterpret_cast<void*>(GetProcAddress(shell,"SHGetSpecialFolderPathA")),reinterpret_cast<void*>(special_folder_path),reinterpret_cast<void**>(&original_special_folder_path));
}
}
