# Layered COM hook lifecycle

A private isolated startup probe reproduced a stack overflow (`0xc00000fd`)
during the second D3D8 device creation. The captured stack alternated between
Windows Apphelp compatibility forwarding and the bridge's device Release hook.
The previously installed bridge also reproduced this failure.

The bridge had treated a newly observed forwarding shim as its original Release
implementation. That shim forwarded to the already installed bridge hook,
creating a cycle. `native/hook_chain.h` keeps the first original dispatch pointer
immutable and preserves later unknown layers instead of installing over them.
The same rule covers graphics vtables, surface descriptions, backend probes and
the DirectInput proxy.

`tests/native_layered_com.cpp` creates a device, adds a forwarding Release shim,
then creates a second device. Its bounded recursion guard fails with the earlier
bridge and passes with the correction, including zero-reference cleanup. This
GPU regression runs through `tools/run_offscreen.py`; the input desktop remains
unchanged. A separate 35-second original-game startup probe passed with the fix.

This establishes the reproduced forwarding-chain case. It does not establish
compatibility with every third-party COM implementation or complete-match and
device-reset acceptance.
