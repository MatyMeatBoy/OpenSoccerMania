#pragma once
namespace osm {
enum class HookDecision { Install, AlreadyInstalled, PreserveLayer };
// A later COM shim may save our hook and replace the slot with a forwarding
// thunk. Saving that thunk as our original creates hook -> shim -> hook.
// Keep the first original immutable and let a newly layered shim call through.
template<class T> inline HookDecision hook_decision(T current,T replacement,T original) {
    if(current==replacement)return HookDecision::AlreadyInstalled;
    if(original&&current!=original)return HookDecision::PreserveLayer;
    return HookDecision::Install;
}
}
