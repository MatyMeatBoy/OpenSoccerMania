<div align="center">

# OpenSoccerMania

**Small bricks. Big match energy.**

![Status: experimental](https://img.shields.io/badge/status-experimental_alpha-e4ad45?style=for-the-badge)
![Windows](https://img.shields.io/badge/platform-Windows_10_%2F_11-357ec7?style=for-the-badge&logo=windows)
![Direct3D 11](https://img.shields.io/badge/renderer-Direct3D_11-328454?style=for-the-badge)
![Python and C++](https://img.shields.io/badge/code-Python_%2B_C%2B%2B-566e83?style=for-the-badge)
![Bring your own game](https://img.shields.io/badge/data-bring_your_own_game-a55b42?style=for-the-badge)

A preservation and modernization project for **LEGO Soccer Mania / Football Mania (PC, 2002)**.

[Prereleases](https://github.com/MatyMeatBoy/OpenSoccerMania/releases) · [Changelog](CHANGELOG.md) · [Getting started](#getting-started) · [Graphics](#graphics) · [Development](#development) · [Current limits](#current-limits)

</div>

OpenSoccerMania gives your own game files a new desktop launcher and an incremental path toward an open reimplementation. The current build runs the original x86 game with version-checked compatibility patches and a native rendering bridge. It is **not a completed decompilation or static recompilation**.

## What works today

| Area | Implementation and evidence |
|---|---|
| Launcher | English interface, rounded controls, textured football artwork and local settings. |
| Import | Private folder imports and content-checked ISO9660/raw-sector images, with read-only mounting or bounded extraction. See [supported layouts and limits](docs/disc-import.md); real image acceptance remains open. |
| Preservation | Private copies, executable hashes, original backups and reversible version-locked startup patches. |
| Startup | The owner's supported private copy reaches menus and active gameplay. A complete match and save/reload remain acceptance work. |
| Modern output | D3D8 commands translated to Direct3D 11 through the user-supplied dgVoodoo wrapper; native instrumentation confirmed 1920×1080 render targets with 4× MSAA. |
| Window modes | Windowed by default, borderless and explicit fullscreen. Windowed startup was checked with unchanged desktop dimensions. |
| Languages | Nine text variants and eight voice directories, including Spanish. American and English share one English voice directory; selection behavior remains under investigation. |

## Getting started

**Current prerelease: 0.1.0-pre.1 — experimental source snapshot, no standalone installer.** See [scope and verification](docs/releases/0.1.0-pre.1.md).

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
**a disc image**. The launcher imports into `local/library/`; your source remains
unchanged. Image support is detected from content, including bounded single-track
raw layouts and restricted CUE files. Extraction requires 7-Zip when read-only
Windows mounting is unavailable. Damaged or zero-filled executables require a
supported executable donor; usable assets are retained in the private import.

The current graphics path requires dgVoodoo's x86 D3D8 wrapper and configuration.
Imports reuse a validated owner-supplied backend already in the private library,
or ask for its DLL/configuration. The bridge preserves that wrapper as
`d3d8_backend.dll`. Obtain dgVoodoo from its
[official site](https://dege.freeweb.hu/dgVoodoo2/). Existing unknown graphics or
input DLLs are never silently overwritten.

## Graphics

- Configurable internal rendering up to 3840 × 2160 and ultrawide formats. 1920×1080 target allocation has been observed; 4K still needs hardware acceptance. Original low-resolution cinematics retain their source detail.
- Display requests through 240 Hz. A display refresh request is not a claim of 240 independent gameplay frames per second.
- MSAA through 16× when supported by the GPU, and up to 16× anisotropic filtering.
- Windowed, borderless and exclusive fullscreen presentation. F11 / Alt+Enter toggle Windowed and Borderless while preserving the internal render resolution.
- Aspect-preserving default presentation. Experimental Hor+ work is incomplete, especially shader-based HUD and cinematic handling.
- Version-checked Hor+ lens conversion preserves vertical framing; background-layer raster/depth/fog corrections address the black sky, and a separate sprite path addresses tiled-image seams. See [render-layer investigation](docs/render-layer-corrections.md).
- Generated blue title sidebars for 16:9 and ultrawide, with private edge-color matching for both regional titles. The original 4:3 artwork remains unchanged. See [title artwork and verification](docs/title-branding.md); transition coverage remains experimental.

The intended video policy is matching black backgrounds for opening logos, original aspect ratios for cinematics, and integer scaling where practical. HUD elements must keep their shape and adjust to the selected aspect ratio. These are acceptance requirements, not completed feature claims.

VRR, FreeSync and G-SYNC depend on monitor/driver configuration. Activation has not been measured here. VRS and temporal techniques such as TAA/DLAA need renderer information and integration that this build does not yet provide.

## Development

```powershell
.venv\Scripts\python -m unittest discover -s tests -v
.\tools\test_native.ps1 -Toolchain "path\to\llvm-mingw"
```

These commands run fixture-based Python and native checks without opening the game. Window verification tools start game windows; use the isolated-desktop harness when tests must not interrupt the desktop. Local evidence and game assets stay outside source control.

Native code lives in `native/`; import, guarded patching, configuration and the launcher live in `opensoccermania/`. Version-checked engine hooks separate the render wait from the original simulation rate. English and American modes both selected 50 Hz and were observed near 50 simulation steps/s with rendering above that rate. See [timing evidence](docs/engine-timing.md) and the [work board](docs/WORKBOARD.md) for limits and pending checks.

## Current limits

The adaptive HUD, per-video scaling, modern controller bridge, performance overlay and high-FPS/simulation comparison are active work. Physical controller compatibility, real ISO import, full-match timing, 4K target allocation and hardware VRR acceptance must be checked before a release claims support.

Some character animations and goalkeeper motion were reported to look too fast or jittery at high render rates. Development after pre.1 corrects an identified interpolation-boundary rewind; see [animation evidence](docs/animation-interpolation.md) and the unreleased changelog. Full animation equivalence and locked 240-FPS gameplay remain unverified. Some replay cameras still show a black sky/horizon band. Development tests use an isolated Win32 desktop with muted test-process audio and private input, without activating windows on the user's desktop.

## Credits

- Launcher styling adapted from the owner's [NewSnapRecomp](https://github.com/MatyMeatBoy/NewSnapRecomp), with its MIT notice retained in `licenses/`.
- [dgVoodoo2](https://github.com/dege-diosg/dgVoodoo2) by Dege supplies the legacy DirectX translation used by the owner's game copy.
- Nunito is distributed under the SIL Open Font License in `licenses/Nunito-OFL.txt`.

OpenSoccerMania is an independent fan project. LEGO and the original game's names and assets belong to their respective owners.

Project code is provided under [MIT](LICENSE); see [third-party notices](THIRD_PARTY.md) for separate licenses.
