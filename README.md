<div align="center">

<img src="assets/branding/icon.png" width="96" height="96" alt="OpenSoccerMania football and brick icon">

# OpenSoccerMania

**Small bricks. Big match energy.**

![Status: experimental](https://img.shields.io/badge/status-experimental_alpha-e4ad45?style=for-the-badge)
![Windows](https://img.shields.io/badge/platform-Windows_10_%2F_11-357ec7?style=for-the-badge&logo=windows)
![Direct3D 11](https://img.shields.io/badge/renderer-Direct3D_11-328454?style=for-the-badge)
![Python and C++](https://img.shields.io/badge/code-Python_%2B_C%2B%2B-566e83?style=for-the-badge)
![Bring your own game](https://img.shields.io/badge/data-bring_your_own_game-a55b42?style=for-the-badge)

A preservation and modernization project for **LEGO Soccer Mania / Football Mania (PC, 2002)**.

[Prereleases](https://github.com/MatyMeatBoy/OpenSoccerMania/releases) Â· [Changelog](CHANGELOG.md) Â· [Getting started](#getting-started) Â· [Graphics](#graphics) Â· [Development](#development) Â· [Current limits](#current-limits)

</div>

OpenSoccerMania gives your own game files a new desktop launcher and an incremental path toward an open reimplementation. The current build runs the original x86 game with version-checked compatibility patches and a native rendering bridge. It is **not a completed decompilation or static recompilation**.

## What works today

| Area | Implementation and evidence |
|---|---|
| Launcher | English interface, rounded controls, textured football artwork and local settings. |
| Import | Private folder imports and content-checked ISO9660/raw-sector images, with read-only mounting or bounded extraction. See [supported layouts and limits](docs/disc-import.md); real image acceptance remains open. |
| Preservation | Private copies, executable hashes, original backups and reversible version-locked startup patches. |
| Startup | The supported private copy reaches menus and active gameplay. An ice-field match reaches final statistics and returns to Main Menu. The City Park â†’ Western Junction Story checkpoint survives a fresh-process reload. Broader campaign/tournament persistence and load recovery remain open. |
| Modern output | D3D8 commands translated to Direct3D 11 through the bundled dgVoodoo backend; native instrumentation confirmed 1920Ã—1080 render targets with 4Ã— MSAA. |
| Window modes | Windowed by default, borderless and explicit fullscreen. Windowed startup was checked with unchanged desktop dimensions. |
| Languages | Nine text variants and eight voice directories, including Spanish. American and English share one English voice directory; selection behavior remains under investigation. |

## Getting started

**Current prerelease: 0.1.0-pre.8 â€” self-contained Windows portable launcher and source.**
See [scope and verification](docs/releases/0.1.0-pre.8.md).

For the portable build, download the Windows portable ZIP from
[the release page](https://github.com/MatyMeatBoy/OpenSoccerMania/releases/tag/v0.1.0-pre.8),
extract the entire folder and run `OpenSoccerMania.exe` as a normal user.
Keep its accompanying files. The launcher includes Python and its dependencies;
you supply only your own valid game ISO or game folder. The graphics backend,
default configuration and supported ISO reader are included; no separate
dgVoodoo or 7-Zip installation is needed. This build targets
Windows x64; the original game and compatibility bridges remain x86.

Administrator permission is not required. A writable portable folder keeps data
in `local/` beside the launcher. If that location is protected, the launcher uses
`%LOCALAPPDATA%\OpenSoccerMania` for its library, preferences and logs. Game
profiles and legacy registry settings stay inside the private game copy; no
machine-wide installation or registry write is needed. The Compatibility page
opens the active data/log folders. An existing library in a protected location
is preserved; import your owned source into the writable library to use it there.

For development from source:

You need your own PC game files. Original executables, discs, voices and other game assets are not part of this project's source distribution.

```powershell
git clone https://github.com/MatyMeatBoy/OpenSoccerMania.git
cd OpenSoccerMania
py -3.11 -m venv .venv
.venv\Scripts\python -m pip install -r requirements.txt
```

Build the native components first, using LLVM-MinGW:

```powershell
powershell -ExecutionPolicy Bypass -File tools/build_native.ps1 -Toolchain "path\to\llvm-mingw"
```

Then run `.venv\Scripts\python -m opensoccermania.launcher`. Python 3.11 with Tkinter, Pillow, Windows 10/11 and LLVM-MinGW are required. MinHook is vendored; SDL3 is included with its license.

Choose **a folder** containing an extracted disc or installed game, or choose
**a disc image**. The launcher imports into the active data folder's `library/`; your source remains
unchanged. Image support is detected from content, including bounded single-track
raw layouts and restricted CUE files. Supported images use the included ISO9660/Joliet
reader. Damaged or zero-filled executables require a
supported executable donor; usable assets are retained in the private import.

The portable includes an unmodified dgVoodoo 2.87.6 x86 backend and its default
configuration for this specific game modification. Imports install them into the
private game copy automatically, including on a fresh PC with an empty library.
The bridge preserves its backend as `d3d8_backend.dll`. Existing unknown graphics
or input DLLs are never silently overwritten. No graphics-component file picker
or separate download is part of the normal import flow.

## Graphics

- Configurable internal rendering up to 3840 x 2160 and ultrawide formats. Both 1920x1080 and 3840x2160 target allocation have been observed. Live 4K presentation and hardware acceptance remain pending. Original low-resolution cinematics retain their source detail.
- Display requests through 240 Hz. A display refresh request is not a claim of 240 independent gameplay frames per second.
- MSAA through 16Ã— when supported by the GPU, and up to 16Ã— anisotropic filtering.
- Windowed, borderless and exclusive fullscreen presentation. F11 / Alt+Enter toggle Windowed and Borderless while preserving the internal render resolution.
- Widescreen is enabled by default, preserving the original vertical framing and camera position. Original-ratio presentation remains selectable. The wider world view and centered UI have selected scene checks; broader HUD, cinematic and camera-transition acceptance remains open.
- Version-checked Hor+ lens conversion preserves vertical framing; background-layer raster/depth/fog corrections address the black sky, and a separate sprite path addresses tiled-image seams. See [render-layer investigation](docs/render-layer-corrections.md).
- Generated blue title sidebars for 16:9 and ultrawide, with private edge-color matching for both regional titles. The original 4:3 artwork remains unchanged. See [title artwork and verification](docs/title-branding.md); transition coverage remains experimental.
- The version-checked stencil shadow composite covers the full output instead of the old 4:3 center; focused GPU tests preserve ordinary HUD centering. Other shadow/effect paths still need scene coverage.

The intended video policy is matching black backgrounds for opening logos, original aspect ratios for cinematics, and integer scaling where practical. HUD elements must keep their shape and adjust to the selected aspect ratio. These are acceptance requirements, not completed feature claims.

Press **F1** in the game, or **Start + View/Select** on a controller, to open
internal settings. Experimental distant-camera presets are optional; the
original camera framing remains the default. Controllers use A to confirm and
B to go back; connected pads remain separate from the keyboard. See
[controls and device limits](docs/controllers.md).

VRR, FreeSync and G-SYNC depend on monitor/driver configuration. Activation has not been measured here. VRS and temporal techniques such as TAA/DLAA need renderer information and integration that this build does not yet provide.

## Development

```powershell
.venv\Scripts\python -m unittest discover -s tests -v
.\tools\test_native.ps1 -Toolchain "path\to\llvm-mingw"
# Also run asset-free GPU and native input integration on isolated desktops:
.\tools\test_native.ps1 -Toolchain "path\to\llvm-mingw" -Gpu
```

These commands run fixture-based Python and native checks without opening the game. `-Gpu` requires built native DLLs and uses the pinned bundled backend, fresh synthetic fixtures and isolated Win32 desktops; it never switches the input desktop. Reports and captures remain under `build/tests/gpu-*/`. The optional `-Python` argument selects the Python interpreter (the project virtual environment is the default). Window verification tools start game windows; use the isolated-desktop harness when tests must not interrupt the desktop. Local evidence and game assets stay outside source control.

To build the portable launcher, install `PyInstaller==6.22.3` in the build
environment and run `tools/build_launcher.ps1` with the LLVM-MinGW path. Its
candidate folder is `build/portable/OpenSoccerMania/`. Source and portable
packaging tools inventory every distributed file and refuse to replace an
existing archive. The explicit `--smoke-test-report <path>` launcher option
validates bundled resources and opens/closes all four pages using temporary
preferences, without starting a game.

Native code lives in `native/`; import, guarded patching, configuration and the launcher live in `opensoccermania/`. Version-checked engine hooks separate the render wait from the original simulation rate. English and American modes both selected 50 Hz and were observed near 50 simulation steps/s with rendering above that rate. See [timing evidence](docs/engine-timing.md) and the [work board](docs/WORKBOARD.md) for limits and pending checks.

## Current limits

The native HUD/settings, controller bridge and graphics corrections have automated and selected owner-game checks. Physical four-controller compatibility, broad disc-image intake, full-match timer/audio equivalence, live 4K presentation and hardware VRR acceptance remain pending. See the release notes and work board for the exact verified cases.

Some character animations and goalkeeper motion were reported to look too fast or jittery at high render rates. Smooth Mode remains disabled after visual regressions; first-snapshot camera initialization stays active independently. Full animation equivalence and locked 240-FPS gameplay remain unverified. An earlier nearly black replay exit is still open; a later completed match did not reproduce it. Development tests now verify the actual private Win32 desktop, with muted test-process audio and private input, without activating the user desktop. Earlier background-only probes have the limitations documented in the work board.

## Credits

- Original OpenSoccerMania icon/favicons are in `assets/branding/`; regenerate them with `tools/build_icons.py`. The same multi-size icon is embedded in the launcher and native game window without changing original game resources.
- Launcher styling adapted from the owner's [NewSnapRecomp](https://github.com/MatyMeatBoy/NewSnapRecomp), with its MIT notice retained in `licenses/`.
- [dgVoodoo2](https://github.com/dege-diosg/dgVoodoo2) by Dege supplies the legacy DirectX translation used by the owner's game copy.
- Nunito is distributed under the SIL Open Font License in `licenses/Nunito-OFL.txt`.

OpenSoccerMania is an independent fan project. LEGO and the original game's names and assets belong to their respective owners.

Project code is provided under [MIT](LICENSE); see [third-party notices](THIRD_PARTY.md) for separate licenses.
