# Changelog

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
