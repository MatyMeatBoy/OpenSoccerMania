# Changelog

## Unreleased

- Preserve user-resized window dimensions when adaptive presentation changes its source rectangle; explicitly scale into the existing client instead of allowing backend resize feedback.
- Recompute adaptive window raster scales after device creation or Reset even when the fitted presentation rectangle is unchanged; add GPU readback coverage for failed-reset retries and unchanged-window aspect recovery.
- Preserve the caller's Win32 error state across save-list metadata normalization and diagnostic logging.
- Add bounded save-status diagnostics available only with explicit isolated-probe flags; keep normal game behavior and save bytes unchanged.

## 0.1.0-pre.5 - 2026-11-17

- Center animated menu models and their depth-clear regions in the same 4:3 UI area, preserving their original local camera. Scope the correction to verified preview callers, restore viewports after draws, and preserve world cameras and experimental gameplay zooms.
- Guard the verified save-list metadata consumer against null, blank and unterminated results, and prevent a missing team field from scanning past NUL. Preserve valid records and save files.
- Scale logical backbuffer clear rectangles to the physical render target, including depth/stencil clears used by animated menu previews. Preserve offscreen coordinates and avoid transforming presentation bars twice.
- Keep private diagnostic keyboard input exclusive, including incomplete mailbox updates; ordinary input continues to preserve physical keys.
- Detect gameplay from the verified dialog manager instead of resource-loaded flags that stay set during matches, so controller mappings can change correctly between gameplay and pause menus.
- Redirect the game's actual `SHGetSpecialFolderPathA` save path and `RegCreateKeyExA` legacy key creation into its private profile. The earlier bridge covered different API variants and left these real calls outside private storage.
- Capture RGB565 game backbuffers as 32-bit diagnostic BMPs; report unsupported formats and file-write failures instead of logging success without producing an image.
- Add original shared football/brick branding for launcher windows, the portable executable and game windows in all display modes; include SVG/ICO/PNG web favicons and an Apple touch icon.
- Launch private test processes through native Win32 STARTUPINFO.lpDesktop. Assigning an arbitrary lpDesktop attribute to Python's subprocess.STARTUPINFO did not isolate earlier test processes; a child-side regression now checks the actual desktop name.
- Run with user-writable storage automatically: preserve writable portable `local/`, otherwise use `%LOCALAPPDATA%\OpenSoccerMania`. Keep preferences/logs/library writes outside protected install folders and report the actual log path.
- Make newly imported private runtime/configuration copies writable when source files carry the Windows read-only attribute, preserving original bytes and attributes.
- Restore logical backbuffer identity, render-target classification and the full logical viewport after a successful graphics-device reset. An asset-free GPU regression reproduces the previous physical-size leak and checks repeated 4:3/16:9 logical resets and presentation.
- Add `tools/test_native.ps1 -Gpu` for fresh asset-free GPU/input integration using the pinned distributed backend and isolated desktops, with retained reports and captures.

## 0.1.0-pre.3 — 2026-11-16

- Fix clean-PC imports requesting graphics files: include the pinned official dgVoodoo x86 backend and default configuration, install automatically, and remove component file dialogs.
- Preserve the original window-message chain before backend device creation, correcting reproduced recursion on device recreation while retaining F11 toggles.
- Import supported ISO9660/Joliet images without an installed extractor or separate download.
- Check the frozen launcher with a fresh synthetic ISO and no external process creation, covering automatic graphics provisioning and profile save.

## 0.1.0-pre.2 — 2026-11-16

- Add an inventoried Windows x64 portable launcher package, including its Python/runtime dependencies and project x86 bridges, with isolated packaged-UI smoke testing.
- Correct the verified stencil-shadow resolve being compressed into the old 4:3 center; preserve ordinary HUD framing and cover the physical pixel edges.
- Allow experimental camera zoom presets when Hor+ is disabled, retaining the untouched stock path for default non-Hor+ framing.
- Recover failed-import staging containing copied read-only files while preserving original attributes and refusing linked/outside cleanup targets.
- Add generated widescreen title backgrounds, privately matched to the selected Soccer Mania or Football Mania title edges without replacing the original center artwork.
- Prime newly allocated interpolation history after its first complete save. Isolated traces reproduce the previous zero-camera feedback and verify stable original camera aspect/up and 50-Hz simulation after the correction; goal/replay acceptance remains open.
- Add shared native F1 settings, controller Start + View/Select handling, raw button mapping and Xbox navigation hints. Physical-controller acceptance remains open.
- Separate regional presentation from text language, and add guarded Soccer/Football resource selection without changing the simulation clock.
- Extend private imports with bounded ISO9660/raw-sector parsing, executable-donor repair and reuse of owner-supplied graphics backends.
- Preserve existing COM forwarding layers when installing hooks, correcting a reproduced Release recursion during second-device creation.
- Correct the original interpolator's endpoint snap during the first 5% of a tick. Same-process A/B traces recorded 122 within-tick coefficient rewinds in 2400 frames with the old behavior and none in 1800 frames with the correction. Simulation remained near 50 steps/s; this is not a locked-240-FPS result.
- Add a smooth-interpolation graphics option, version-checked animation diagnostics and a corrected SA-state sampler.
- Add a native regression for interpolation continuity and snapshot-bank strides.
- Compare canonical installer paths in tests so Windows 8.3 TEMP aliases work on GitHub runners.
- Correct signed-RHW horizon clipping while preserving sky texture perspective; handle color-only backgrounds and layers straddling the far-depth boundary. GPU regressions cover five buffered/immediate draw paths and restoration of graphics state.
- Remaining reported issues: goal-net/shadow clipping at the original frame, apparent camera zoom and camera transitions after goals. All-map/replay visual acceptance remains open.

## 0.1.0-pre.1 — 2026-11-16

First public experimental source snapshot for Windows. This is an incremental compatibility and modernization project using the owner's original x86 game, not a completed recompilation.

- English textured desktop launcher with private folder/ISO imports, language inventory and local configuration.
- Hash-guarded, reversible startup patches for two identified executable versions.
- Native D3D8/input bridges, with a user-supplied graphics backend for Direct3D 11.
- Internal-resolution, MSAA/anisotropic filtering, window/borderless/fullscreen, F11 and background-running options.
- Experimental Hor+ camera/frustum conversion, HUD/movie composition, sky depth/fog correction and tiled-sprite seam handling.
- Separate render-wait controls, frame-time overlay and diagnostic logs.
- SDL3 controller mapping and private input transport.
- Bounded CPU mirrors for screen-space buffers, shader-state caches and isolated-desktop regression tools.

### Known issues and unverified areas

- Character animations and goalkeeper motion may run too fast or jitter at high render rates. Locked 240-FPS timing equivalence is not verified.
- Sky, shadow, effect and framing coverage across every map/cinematic remains incomplete.
- Full-match/save/reload, real ISO import, physical controllers, 4K allocation and hardware VRR need acceptance testing.
- Isolated game-probe shutdown can time out and require terminating its own test process. This does not establish a clean shutdown regression pass.
- VRS, TAA, DLAA and native x64 gameplay are not implemented. FreeSync/G-SYNC activation depends on hardware and driver settings and is unverified.
- No standalone installer is provided. Game files and dgVoodoo are not distributed.

See [release scope](docs/releases/0.1.0-pre.1.md), [work board](docs/WORKBOARD.md) and [timing evidence](docs/engine-timing.md).
