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

## Backend window-procedure layers

Bundled dgVoodoo 2.87.6 exposed a separate forwarding-cycle case during actual game startup. The old bridge re-subclassed the same HWND whenever its current procedure differed from `game_proc`. After the backend installed another outer procedure, the bridge captured that forwarding layer as its new downstream, creating a window-message cycle. The isolated crash stack repeatedly passed through `game_proc`, User32 and the backend before stack overflow (`0xc00000fd`) and callback termination (`0xc000041d`). This was not the COM Release cycle above.

Window filtering is now installed **before** the backend's first `CreateDevice` or `Reset` for that HWND. The bridge records the true original downstream per window and leaves later outer layers in place. Window appearance changes only change style and placement. `WM_NCDESTROY` removes the window's record after forwarding destruction to its original procedure.

The asset-free `tests/native_layered_window.cpp` adds a forwarding WndProc with bounded recursion detection. The old bridge fails when an appearance update recaptures that layer. The corrected bridge passes on both the bundled 2.87.6 backend and the previously tested 2.79 backend, including repeated device creation, original-message dispatch exactly once and F11 toggles in both directions. All messages target only the regression's own isolated window.

A fresh 40-second original-game startup probe with bundled 2.87.6 then passed two device creations, reached movie playback and exceeded 1,200 presents without a first-chance exception. Its saved downstream was the actual game procedure `0x00449d80`, rather than a backend thunk. Private evidence is recorded in `local/backend287-evidence/window-chain-evidence.json`. The owned probe required bounded forced cleanup after its close request; the input desktop remained unchanged. This establishes the startup and window-chain regression, not complete-match acceptance.
