#pragma once
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <initializer_list>
#include "../third_party/minhook/include/MinHook.h"

namespace osm {
enum class Edition { Soccer, Football };
inline Edition select_edition(const char* edition,const char* language) {
    if(edition && _stricmp(edition,"SOCCER")==0)return Edition::Soccer;
    if(edition && _stricmp(edition,"FOOTBALL")==0)return Edition::Football;
    return language && _stricmp(language,"AMERICAN")==0?Edition::Soccer:Edition::Football;
}
inline Edition configured_edition(const char* config) {
    char edition[32],language[32];
    GetPrivateProfileStringA("Game","Edition","",edition,sizeof(edition),config);
    GetPrivateProfileStringA("Game","Language","ENGLISH",language,sizeof(language),config);
    return select_edition(edition,language);
}
inline bool edition_callsite(std::uintptr_t address) {
    return address==0x44a49e || address==0x4eed38 || address==0x4f2e7e || address==0x4f9f1f;
}
inline int edition_comparison_result(Edition edition,std::uintptr_t caller,int original) {
    return edition_callsite(caller)?(edition==Edition::Soccer?0:1):original;
}
inline bool edition_image_signature(const unsigned char* base,size_t size,bool verified) {
    if(!verified || !base || size<0x142810+24)return false;
    constexpr unsigned char compare[]={0x55,0x8b,0xec,0x57,0x56,0x53,0x8b,0x75,0x0c,0x8b,0x7d,0x08,0x8d,0x05,0x94,0x97,0x68,0x00,0x83,0x78,0x08,0x00,0x75,0x3b};
    constexpr size_t offsets[]={0x4a499,0xeed33,0xf2e79,0xf9f1a};
    constexpr unsigned char calls[][5]={{0xe8,0x72,0x83,0x0f,0},{0xe8,0xd8,0x3a,0x05,0},{0xe8,0x92,0xf9,0x04,0},{0xe8,0xf1,0x88,0x04,0}};
    if(std::memcmp(base+0x142810,compare,sizeof(compare)))return false;
    for(unsigned i=0;i<4;++i)if(offsets[i]+5>size || std::memcmp(base+offsets[i],calls[i],5))return false;
    return true;
}
inline bool edition_runtime_gate(const char* config) {
    if(!GetPrivateProfileIntA("Graphics","VerifiedExecutable",0,config))return false;
    const auto* base=reinterpret_cast<const unsigned char*>(GetModuleHandleA(nullptr));
    if(reinterpret_cast<std::uintptr_t>(base)!=0x400000)return false;
    const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<0 || static_cast<size_t>(dos->e_lfanew)>0x1000-sizeof(IMAGE_NT_HEADERS))return false;
    const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS*>(base+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC || nt->OptionalHeader.SizeOfImage<0x142828)return false;
    // Every inspected instruction range must belong to committed readable image memory.
    for(size_t offset:{size_t(0x142810),size_t(0x4a499),size_t(0xeed33),size_t(0xf2e79),size_t(0xf9f1a)}) {
        MEMORY_BASIC_INFORMATION memory={};
        if(!VirtualQuery(base+offset,&memory,sizeof(memory)) || memory.State!=MEM_COMMIT || memory.Type!=MEM_IMAGE ||
           (memory.Protect&(PAGE_NOACCESS|PAGE_GUARD)) ||
           reinterpret_cast<std::uintptr_t>(base+offset+24)>reinterpret_cast<std::uintptr_t>(memory.BaseAddress)+memory.RegionSize)return false;
    }
    return edition_image_signature(base,nt->OptionalHeader.SizeOfImage,true);
}
inline bool edition_early_lookup_context(bool verified,const void* const* frames,size_t count) {
    if(!verified || !frames)return false;
    for(size_t i=0;i<count && i<8;++i)if(reinterpret_cast<std::uintptr_t>(frames[i])==0x44a493)return true;
    return false;
}
using EditionStackCapture=USHORT (WINAPI *)(DWORD,DWORD,PVOID*,PDWORD);
inline const char* edition_early_language(Edition edition,bool verified,EditionStackCapture capture) {
    if(!verified || !capture)return nullptr;
    void* frames[8]={};
    const auto count=capture(0,8,frames,nullptr);
    return edition_early_lookup_context(verified,frames,count)?(edition==Edition::Soccer?"AMERICAN":"ENGLISH"):nullptr;
}
inline const char* edition_registry_language(const char* config) {
    return edition_early_language(configured_edition(config),edition_runtime_gate(config),CaptureStackBackTrace);
}
using EditionCompare=int (__cdecl *)(const char*,const char*);
static EditionCompare original_edition_compare=nullptr;
static Edition active_edition=Edition::Football;
static int __cdecl edition_compare(const char* left,const char* right) {
    const auto caller=reinterpret_cast<std::uintptr_t>(__builtin_return_address(0));
    if(edition_callsite(caller))return edition_comparison_result(active_edition,caller,0);
    return original_edition_compare(left,right);
}
inline void start_edition_hooks(const char* config,const char* log_path) {
    if(!edition_runtime_gate(config))return;
    active_edition=configured_edition(config);
    const auto result=MH_CreateHook(reinterpret_cast<void*>(0x542810),reinterpret_cast<void*>(edition_compare),reinterpret_cast<void**>(&original_edition_compare));
    FILE* log=log_path?std::fopen(log_path,"a"):nullptr;
    if(log){std::fprintf(log,"EDITION region=%s compare-hook=%d locale independent\n",active_edition==Edition::Soccer?"SOCCER":"FOOTBALL",int(result));std::fclose(log);}
}
} // namespace osm
