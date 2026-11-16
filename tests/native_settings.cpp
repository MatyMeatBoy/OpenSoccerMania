#define CINTERFACE
#define COBJMACROS
#include "../native/settings_menu.h"
#include <cassert>
#include <string>
static SHORT keyboard[256]={};
static SHORT key(int code){return keyboard[code];}
int main() {
    char temporary[MAX_PATH];GetTempPathA(MAX_PATH,temporary);
    std::string directory=std::string(temporary)+"OpenSoccerMania-Settings-"+std::to_string(GetCurrentProcessId())+"\\";
    CreateDirectoryA(directory.c_str(),nullptr);
    const std::string ini=directory+"opensoccermania.ini",wrapper=directory+"dgVoodoo.conf";
    FILE* file=std::fopen(wrapper.c_str(),"wb");assert(file);std::fputs("Version=0x279\n[GeneralExt]\nFPSLimit=240\n",file);std::fclose(file);
    {
        osm::SettingsMenu menu;osm::UiStateReader reader;menu.configure(directory.c_str(),ini.c_str());
        assert(menu.zoom_index()==0&&!menu.is_open());
        keyboard[VK_F1]=0x8000;menu.update(key,true,true);assert(menu.is_open()&&reader.panel_open());
        menu.update(key,true,true);assert(menu.is_open()); // No repeat while held.
        keyboard[VK_F1]=0;menu.update(key,true,true);
        keyboard[VK_RIGHT]=0x8000;menu.update(key,true,true);assert(menu.zoom_index()==1);
        assert(GetPrivateProfileIntA("Graphics","CameraZoom",0,ini.c_str())==1);
        keyboard[VK_RIGHT]=0;menu.update(key,true,true);
        keyboard[VK_NEXT]=0x8000;menu.update(key,true,true);keyboard[VK_NEXT]=0;menu.update(key,true,true);
        keyboard[VK_RIGHT]=0x8000;menu.update(key,true,true);keyboard[VK_RIGHT]=0;menu.update(key,true,true);
        assert(GetPrivateProfileIntA("Graphics","RenderFPS",0,ini.c_str())==360);
        assert(GetPrivateProfileIntA("GeneralExt","FPSLimit",0,wrapper.c_str())==360);
        file=std::fopen(wrapper.c_str(),"rb");char buffer[256]={};assert(file);std::fread(buffer,1,255,file);std::fclose(file);assert(std::strstr(buffer,"Version=0x279"));
        menu.update(key,false,true);assert(!menu.is_open()&&!reader.panel_open());
    }
    DeleteFileA(ini.c_str());DeleteFileA(wrapper.c_str());RemoveDirectoryA(directory.c_str());
}
