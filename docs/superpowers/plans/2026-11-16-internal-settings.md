# Internal settings implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a controller-friendly native settings panel and shared camera preferences without changing authoritative gameplay timing or activating desktop windows.

**Architecture:** A native render overlay publishes a 32-byte per-process UI state page. SDL input and the DirectInput bridge consume that state to route panel navigation safely. Runtime INI preferences synchronize back to the launcher. Default camera positions/targets remain original; optional lens zoom presets are explicit.

**Tech Stack:** C++17/D3D8 bridge/GDI texture, Win32 shared memory, Python/Tkinter/SDL3.

- [x] Implement controller chord and menu input with no desktop key synthesis; validate edge/release behavior.
- [x] Implement UI-state ownership and keyboard/mouse suppression; validate late publisher, invalid mappings and cleanup.
- [x] Verify projection lens units from original code; test vertical framing for original zoom and optional distant lenses.
- [ ] Implement native panel rendering, focus loss, navigation and safe input release if rendering fails.
- [ ] Wire camera zoom, HUD, interpolation and restart-required render limit to native/launcher settings; synchronize persisted values.
- [ ] Integrate disc-image and damaged-executable import flows plus title/edition controls in the launcher.
- [ ] Integrate generated sidebars only in the original title pass, preserving original pixels and aspect ratio.
- [ ] Run focused native/Python checks and isolated integration. Commit and push independently validated blocks; retain open goal-camera and world-effect acceptance until reproduced and checked.
