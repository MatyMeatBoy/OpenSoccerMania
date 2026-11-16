#include "../native/hook_chain.h"
#include <cassert>
using Method=int (*)();
static Method original=nullptr,shim_next=nullptr;
static int backend_calls=0,hook_calls=0,shim_calls=0;
static int backend(){++backend_calls;return 7;}
static int other_backend(){return 11;}
static int hook(){++hook_calls;return original();}
static int shim(){++shim_calls;return shim_next();}
static void install(Method& slot) {
    if(osm::hook_decision(slot,hook,original)!=osm::HookDecision::Install)return;
    if(!original)original=slot;slot=hook;
}
int main() {
    Method slot=backend;install(slot);assert(original==backend&&slot==hook);
    shim_next=slot;slot=shim; // Windows apphelp / another COM layer wraps us.
    install(slot);assert(slot==shim&&original==backend);
    assert(slot()==7&&backend_calls==1&&hook_calls==1&&shim_calls==1);
    Method another_slot=backend;install(another_slot);assert(another_slot==hook&&original==backend);
    assert(another_slot()==7&&backend_calls==2&&hook_calls==2&&shim_calls==1);
    Method incompatible=other_backend;install(incompatible);assert(incompatible==other_backend&&original==backend);
    assert(incompatible()==11);
    assert(osm::hook_decision(hook,hook,original)==osm::HookDecision::AlreadyInstalled);
}
