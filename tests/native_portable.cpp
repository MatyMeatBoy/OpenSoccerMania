// Virtual registry/profile writes must stay in the private, user-writable copy.
#include "../native/portable.h"
#include <cassert>
static unsigned machine_calls=0;
static LSTATUS WINAPI denied_open(HKEY,LPCSTR,DWORD,REGSAM,PHKEY){++machine_calls;return ERROR_ACCESS_DENIED;}
static LSTATUS WINAPI denied_set(HKEY,LPCSTR,DWORD,DWORD,const BYTE*,DWORD){++machine_calls;return ERROR_ACCESS_DENIED;}
int main(){
    using namespace osm;
    char temporary[MAX_PATH],folder[MAX_PATH];GetTempPathA(MAX_PATH,temporary);
    assert(GetTempFileNameA(temporary,"osm",0,folder));assert(DeleteFileA(folder));assert(CreateDirectoryA(folder,nullptr));
    std::string directory=std::string(folder)+"\\",config=directory+"opensoccermania.ini";
    assert(WritePrivateProfileStringA("Game","Edition","FOOTBALL",config.c_str()));
    prepare_portable(directory.c_str(),config.c_str());
    original_open_registry=denied_open;original_set_registry=denied_set;
    HKEY key=nullptr;
    assert(open_registry(HKEY_LOCAL_MACHINE,"Software\\LEGO Media\\Football Mania",0,KEY_ALL_ACCESS,&key)==ERROR_SUCCESS);
    assert(key==virtual_key&&machine_calls==0);
    HKEY created=nullptr;DWORD disposition=0;
    assert(create_registry(HKEY_LOCAL_MACHINE,"Software\\LEGO Media\\Football Mania",0,nullptr,0,KEY_ALL_ACCESS,nullptr,&created,&disposition)==ERROR_SUCCESS);
    assert(created==virtual_key&&disposition==REG_OPENED_EXISTING_KEY&&machine_calls==0);
    DWORD value=0x12345678;
    assert(set_registry(key,"TestSetting",0,REG_DWORD,reinterpret_cast<const BYTE*>(&value),sizeof(value))==ERROR_SUCCESS);
    DWORD actual=0,size=sizeof(actual),type=0;
    assert(query_registry(key,"TestSetting",nullptr,&type,reinterpret_cast<BYTE*>(&actual),&size)==ERROR_SUCCESS);
    assert(actual==value&&type==REG_DWORD&&size==sizeof(value)&&machine_calls==0);
    assert(close_registry(key)==ERROR_SUCCESS);
    char profile[MAX_PATH]={};
    assert(folder_path(nullptr,0x1a,nullptr,0,profile)==S_OK);
    assert(std::string(profile)==directory+"profile");
    assert(folder_path(nullptr,0x1c,nullptr,0,profile)==S_OK);
    std::memset(profile,0,sizeof(profile));
    assert(special_folder_path(nullptr,profile,0x1a,TRUE));
    assert(std::string(profile)==directory+"profile");
    assert(!special_folder_path(nullptr,nullptr,0x1a,TRUE));
    std::string save=std::string(profile)+"\\save-test.dat";
    FILE* file=std::fopen(save.c_str(),"wb");assert(file);assert(std::fwrite(&value,sizeof(value),1,file)==1);std::fclose(file);
    assert(DeleteFileA(save.c_str()));assert(DeleteFileA(legacy_ini));assert(RemoveDirectoryA(profile));
    assert(DeleteFileA(config.c_str()));assert(RemoveDirectoryA(folder));
    std::puts("PASS: private registry roundtrip and save path without machine registry access");
}
