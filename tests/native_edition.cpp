#include "../native/edition.h"
#include "../native/portable.h"
#include <cassert>
#include <vector>

static unsigned capture_calls=0;
static bool include_region_frame=true;
static USHORT WINAPI mock_stack(DWORD skip,DWORD count,PVOID* frames,PDWORD) {
    assert(skip==0 && count==8);++capture_calls;
    frames[0]=reinterpret_cast<void*>(0x44a7e0);
    frames[1]=reinterpret_cast<void*>(include_region_frame?0x44a493:0x44a494);
    return 2;
}
int main() {
    using namespace osm;
    for(const char* locale:{"AMERICAN","ENGLISH","FRENCH","GERMAN","ITALIAN","SPANISH","DUTCH","DANISH","SWEDISH"}) {
        assert(select_edition("SOCCER",locale)==Edition::Soccer);
        assert(select_edition("FOOTBALL",locale)==Edition::Football);
    }
    assert(select_edition("","AMERICAN")==Edition::Soccer);
    assert(select_edition(nullptr,"ENGLISH")==Edition::Football);
    assert(select_edition("invalid",nullptr)==Edition::Football);
    for(auto address:{0x44a49e,0x4eed38,0x4f2e7e,0x4f9f1f}) {
        assert(edition_callsite(address));
        assert(edition_comparison_result(Edition::Soccer,address,-12)==0);
        assert(edition_comparison_result(Edition::Football,address,0)==1);
        assert(edition_comparison_result(Edition::Soccer,address+1,-12)==-12);
        assert(!edition_callsite(address-1));assert(!edition_callsite(address+1));
    }
    std::vector<unsigned char> image(0x142828);
    const unsigned char signature[]={0x55,0x8b,0xec,0x57,0x56,0x53,0x8b,0x75,0x0c,0x8b,0x7d,0x08,0x8d,0x05,0x94,0x97,0x68,0x00,0x83,0x78,0x08,0x00,0x75,0x3b};
    std::memcpy(image.data()+0x142810,signature,sizeof(signature));
    const size_t offsets[]={0x4a499,0xeed33,0xf2e79,0xf9f1a};
    const unsigned char calls[][5]={{0xe8,0x72,0x83,0x0f,0},{0xe8,0xd8,0x3a,0x05,0},{0xe8,0x92,0xf9,0x04,0},{0xe8,0xf1,0x88,0x04,0}};
    for(unsigned i=0;i<4;++i)std::memcpy(image.data()+offsets[i],calls[i],5);
    assert(edition_image_signature(image.data(),image.size(),true));
    assert(!edition_image_signature(image.data(),image.size(),false));
    assert(!edition_image_signature(image.data(),image.size()-1,true));
    for(size_t offset:{size_t(0x142810),offsets[0],offsets[1],offsets[2],offsets[3]}) {
        image[offset]^=1;
        assert(!edition_image_signature(image.data(),image.size(),true));
        image[offset]^=1;
    }
    assert(!edition_early_language(Edition::Soccer,false,mock_stack) && capture_calls==0);
    assert(std::strcmp(edition_early_language(Edition::Soccer,true,mock_stack),"AMERICAN")==0);
    assert(std::strcmp(edition_early_language(Edition::Football,true,mock_stack),"ENGLISH")==0);
    include_region_frame=false;
    assert(!edition_early_language(Edition::Soccer,true,mock_stack));
    const void* deep[9]={};deep[8]=reinterpret_cast<void*>(0x44a493);
    assert(!edition_early_lookup_context(true,deep,9));
    char directory[MAX_PATH],config[MAX_PATH];GetTempPathA(MAX_PATH,directory);
    assert(GetTempFileNameA(directory,"osm",0,config));
    std::snprintf(portable_config,sizeof(portable_config),"%s",config);
    assert(WritePrivateProfileStringA("Game","Edition","SOCCER",config));
    assert(WritePrivateProfileStringA("Game","Language","SPANISH",config));
    assert(selected_legacy_name("Software\\LEGO Media\\Soccer Mania"));
    assert(!selected_legacy_name("Software\\LEGO Media\\Football Mania"));
    char locale[32]={};DWORD size=sizeof(locale),type=0;
    assert(query_registry(virtual_key,"Language",nullptr,&type,reinterpret_cast<BYTE*>(locale),&size)==ERROR_SUCCESS);
    assert(std::strcmp(locale,"SPANISH")==0 && type==REG_SZ);
    assert(!edition_runtime_gate(config)); // unsigned executable/config remains untouched
    DeleteFileA(config);
    std::puts("native edition: selection, signatures, context, locale isolation passed");
}
