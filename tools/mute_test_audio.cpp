// Mute only the explicitly supplied private test PID, leaving other sessions alone.
#include <windows.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <audioclient.h>
#include <cstdlib>
int main(int argc,char** argv) {
    if(argc!=2)return 1;
    const DWORD wanted=static_cast<DWORD>(std::strtoul(argv[1],nullptr,10));if(!wanted)return 1;
    CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    IMMDeviceEnumerator* enumerator=nullptr;IMMDevice* endpoint=nullptr;IAudioSessionManager2* manager=nullptr;IAudioSessionEnumerator* sessions=nullptr;
    unsigned found=0;
    if(SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_INPROC_SERVER,__uuidof(IMMDeviceEnumerator),reinterpret_cast<void**>(&enumerator)))&&
       SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender,eMultimedia,&endpoint))&&
       SUCCEEDED(endpoint->Activate(__uuidof(IAudioSessionManager2),CLSCTX_INPROC_SERVER,nullptr,reinterpret_cast<void**>(&manager)))&&
       SUCCEEDED(manager->GetSessionEnumerator(&sessions))) {
        int count=0;sessions->GetCount(&count);
        for(int i=0;i<count;++i) {
            IAudioSessionControl* session=nullptr;IAudioSessionControl2* control=nullptr;ISimpleAudioVolume* volume=nullptr;
            if(SUCCEEDED(sessions->GetSession(i,&session))&&SUCCEEDED(session->QueryInterface(__uuidof(IAudioSessionControl2),reinterpret_cast<void**>(&control)))) {
                DWORD pid=0;
                if(SUCCEEDED(control->GetProcessId(&pid))&&pid==wanted&&SUCCEEDED(session->QueryInterface(__uuidof(ISimpleAudioVolume),reinterpret_cast<void**>(&volume)))) {
                    if(SUCCEEDED(volume->SetMute(TRUE,nullptr)))++found;
                }
            }
            if(volume)volume->Release();if(control)control->Release();if(session)session->Release();
        }
    }
    if(sessions)sessions->Release();if(manager)manager->Release();if(endpoint)endpoint->Release();if(enumerator)enumerator->Release();CoUninitialize();
    return found?0:2;
}
