#include "../native/controller_prompts.h"
#include <cassert>
#include <fstream>
#include <iterator>
int main(int argc,char** argv) {
    assert(argc==2);
    assert(osm::prompt_family_from_flags(0x404)==4);
    assert(osm::prompt_family_from_flags(0x704)==1);
    assert(osm::resolve_prompt_family(0,2)==2);
    assert(osm::resolve_prompt_family(3,2)==3);
    assert(osm::resolve_prompt_family(99,99)==1);
    assert(osm::prompt_family_from_flags(0x204)==2);
    assert(osm::prompt_family_from_flags(0x304)==3);
    assert(osm::prompt_family_from_flags(4)==1);
    assert(std::strcmp(osm::prompt_face_label(3,0),"B")==0);
    assert(std::strcmp(osm::prompt_face_label(3,1),"A")==0);
    assert(std::strcmp(osm::prompt_face_label(2,0),"Cross")==0);
    std::ifstream file(argv[1],std::ios::binary);assert(file);
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)),{});
    osm::ControllerPromptAtlas atlas(bytes.data(),bytes.size());assert(atlas.valid());
    std::vector<std::vector<DWORD>> images;
    for(unsigned family=1;family<=4;++family)for(unsigned button=0;button<4;++button) {
        auto image=osm::controller_prompt_pixels(atlas,family,button);
        assert(image.size()==4096&&image[0]==0&&image.back()==0);
        for(const auto& previous:images)assert(image!=previous);
        images.push_back(image);
    }
    bytes.back()^=1;
    assert(!osm::ControllerPromptAtlas(bytes.data(),bytes.size()).valid());
    assert(!osm::ControllerPromptAtlas(bytes.data(),31).valid());
    assert(!osm::ControllerPromptAtlas(nullptr,0).valid());
    assert(osm::controller_prompt_pixels(osm::ControllerPromptAtlas(nullptr,0),1,0).empty());
}
