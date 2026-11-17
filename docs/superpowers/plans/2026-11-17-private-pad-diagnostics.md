# Private virtual-pad diagnostics

Purpose: unblock four-seat owner-game acceptance on a real private Win32 desktop
without activating or switching the input desktop. Normal foreground ownership
remains mandatory. Reuse the existing PID-scoped keyboard diagnostic protocol;
no new launcher preference or physical-input injection.

1. Extend `tests/native_virtual_pad.cpp` with a private keyboard mapping. Verify
   missing, disabled, normal enabled, wrong magic and odd-sequence pages cannot
   authorize background acquisition; enabled+diagnostic permits independent pad
   state and revocation immediately restores DIERR_INPUTLOST with neutral output.
   Run against the old code and record the expected failure.
2. Add a read-only diagnostic ownership reader and use it only in the virtual
   pad focus check. Require a stable packet and flags 1+2, default deny.
   Keep ordinary pad staleness, panel consumption and disconnection unchanged.
3. Run native checks, rebuild both native bridges and run actual-DLL checks.
   Run Python regressions. Repeat owner-game assignment on a private desktop
   with a single producer refreshing all four states; preserve evidence and saves.
4. Review the diff and record exact acceptance limits. This tests the native
   four-slot route, not physical SDL hardware, wireless behavior or an online game.
