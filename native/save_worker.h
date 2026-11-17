#pragma once
#include <windows.h>
#include <cstring>
#include "../third_party/minhook/include/MinHook.h"

namespace osm {
static void* original_save_worker_dispatch=nullptr;
static void* save_worker_idle_continuation=nullptr;

// Mid-function x86 hook: EBX holds the worker's locked command snapshot.
// No C++ calls, prologue or stack changes are allowed here. Nonzero snapshots
// run the original lea/cmp through the trampoline, preserving dispatch flags.
__attribute__((naked)) static void save_worker_dispatch_guard() {
    __asm__ __volatile__(
        "testl %%ebx,%%ebx\n\t"
        "jz 1f\n\t"
        "jmp *%0\n\t"
        "1: jmp *%1\n\t"
        : : "m"(original_save_worker_dispatch),"m"(save_worker_idle_continuation));
}

static bool start_save_worker_guard(unsigned char* base) {
    const unsigned char snapshot[]={0x8b,0x2d,0x0c,0x02,0x57,0x00,0x68,0x08,0xd6,0x61,0x00,0xff,0xd6,0x8b,0x1d,0x38,0xd6,0x61,0x00,0x68,0x08,0xd6,0x61,0x00,0xff,0xd7};
    const unsigned char dispatch[]={0x8d,0x43,0xff,0x83,0xf8,0x16,0x0f,0x87,0x71,0x02,0x00,0x00,0xff,0x24,0x85,0x5c,0x07,0x43,0x00};
    const unsigned char completion[]={0x68,0x08,0xd6,0x61,0x00,0xff,0xd6,0x68,0x08,0xd6,0x61,0x00,0xc7,0x05,0x38,0xd6,0x61,0x00,0x00,0x00,0x00,0x00,0xff,0xd7,0x6a,0x00,0xff,0xd5};
    if(std::memcmp(base+0x30463,snapshot,sizeof(snapshot))||
       std::memcmp(base+0x3047d,dispatch,sizeof(dispatch))||
       std::memcmp(base+0x306fa,completion,sizeof(completion)))return false;
    // An idle snapshot must go straight to Sleep(0), never clear a command
    // that a producer may have published since the snapshot lock was released.
    save_worker_idle_continuation=base+0x30712;
    return MH_CreateHook(base+0x3047d,reinterpret_cast<void*>(save_worker_dispatch_guard),
        &original_save_worker_dispatch)==MH_OK;
}
}
