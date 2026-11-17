#pragma once
#include <cstddef>
#include <cstring>
namespace osm {
// The original list parser copies a 64-byte metadata record onto its stack,
// then skips control separators without checking for NUL. Keep normal records
// intact; represent absent/blank/malformed records with a safe empty string.
inline bool normalize_save_metadata(const char* source,std::size_t available,char (&output)[64]) {
    if(!source){output[0]=0;return true;}
    if(available>64)available=64;
    std::size_t length=0;
    while(length<available&&source[length])++length;
    if(length==available){output[0]=0;return true;}
    if(!length)return false;
    std::size_t separator=0;
    while(separator<length&&static_cast<unsigned char>(source[separator])>=32)++separator;
    if(!separator){output[0]=0;return true;} // An empty name is an unassigned save slot.
    if(separator==length)return false;
    std::size_t tail=separator;
    while(tail<length&&static_cast<unsigned char>(source[tail])<32)++tail;
    if(tail<length)return false;
    // A named record with an absent team has no second string to scan.
    std::memmove(output,source,separator);output[separator]=0;
    return true;
}
}
