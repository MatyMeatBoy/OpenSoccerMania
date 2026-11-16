# Initial reconnaissance — 2026-11-16

Historical static inspection before runtime changes. Later findings are tracked in the work board and release notes.

## Owner-supplied dumps

| Dump | Files | Bytes | Initial observation |
|---|---:|---:|---|
| ElectronicArts-LegoSoccerMania | 5008 | 459712436 | Game.exe and FindDisc.exe contain only zero bytes; unusable executable reference. |
| lego FOOTBALL MANIA | 5010 | 467039704 | Valid five-section x86 PE, 3813376 bytes. |
| Lego Football Mania Dutch | 4998 | 426162097 | Previously modified package with dgVoodoo, batch launcher and third-party NFO. |

Executable SHA-256 identifiers:

- Zero-filled executable: `1e6c483e3281fb69870397e558549573309dc93adffe4b1a37a8d7fa6ed6c61e`
- Football: `b89ce11443b64b0da19e55f88bfe7993a3130e01137ee54517c14896f8cca07c`
- Dutch: `58a98cf9fa332bbc0ec301e16d779c8dbe5497b69778c22e5c363f892fbcb19d`

The valid executables have equal lengths and differ in 3098 bytes. This alone does not establish protection status. Imports include Direct3DCreate8, QueryPerformanceCounter/Frequency, timeGetTime and GetTickCount; imports alone do not establish the simulation clock. The Dutch NFO claims CDilla; this is third-party documentation, not independent detection. Its wrapper initially requested stretched 4:3 at 1920×1440 / 59 Hz.

## Languages

All three contain nine text variants: American, English, Danish, Dutch, French, German, Italian, Spanish and Swedish. Voice directories contain DAN 129, DUT 129, ENG 127, FRE 127, GER 129, ITA 127, SPA 127 and SWE 129 OGG files. American has no separate voice directory and shares English. Presence/count does not certify full dubbing coverage or audio integrity.

## Initial research references

No specific Soccer Mania decompilation was established in the initial searches. These are tools and examples, not verified game compatibility:

| Reference | Potential use and limits |
|---|---|
| [SafeDiscShim](https://github.com/RibShark/SafeDiscShim) | Compatibility reference; requires the original disc and does not remove protection. |
| [SafeDiscLoader2](https://github.com/nckstwrt/SafeDiscLoader2) | Conditional loader reference; does not identify this game's protection. |
| [SafeDiscLoader](https://github.com/nckstwrt/SafeDiscLoader) | Historical predecessor. |
| [DiscCheckEmu](https://github.com/Luca1991/DiscCheckEmu), [DCEConfigs](https://github.com/Luca1991/DCEConfigs) | Disc-check emulation examples; game support unverified. |
| [Detect It Easy](https://github.com/horsicq/Detect-It-Easy) | Compiler/packer/signature investigation. |
| [x64dbg](https://github.com/x64dbg/x64dbg), [Ghidra](https://github.com/NationalSecurityAgency/ghidra) | x86 startup, clock, camera and API investigation. |
| [WidescreenFixesPack](https://github.com/ThirteenAG/WidescreenFixesPack) | Examples of game-specific camera and HUD corrections. |
| [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader), [Hooking.Patterns](https://github.com/ThirteenAG/Hooking.Patterns) | Module-loading and version-checked hook patterns. |
| [d3d8to9](https://github.com/crosire/d3d8to9), [dxwrapper](https://github.com/elishacloud/dxwrapper) | Alternative translation and compatibility references. |
| [DDrawCompat](https://github.com/narzoul/DDrawCompat) | Earlier DirectDraw/D3D generations; not the primary D3D8 route. |
| [isle](https://github.com/isledecomp/isle), [reccmp](https://github.com/isledecomp/reccmp), [LEGOIslandRebuilder](https://github.com/isledecomp/LEGOIslandRebuilder) | Different game/engine; decompilation comparison and launcher examples. |
| [Remill](https://github.com/lifting-bits/remill), [WindowsRecompTool](https://github.com/Zippingrocks/WindowsRecompTool) | Experimental lifting/recompilation references; instruction translation does not reconstruct Win32 services or game flow. |

## Selected direction

Preserve all original inputs. Use private copies with hash/signature-guarded reversible x86 changes while investigating the engine incrementally. Validate camera/HUD/video framing and independent rendering against original simulation behavior. Static x86-to-x64 recompilation remains experimental. Original game content stays outside the public repository.
