# Display modes and Direct3D 11

OpenSoccerMania defaults to **Windowed**. Graphics offers Windowed, Borderless and Fullscreen.

- Windowed: a normal framed window, initially bounded to the desktop work area. The mouse is not captured by the wrapper and the window is not always on top.
- Borderless: a desktop-sized window without borders, using windowed presentation rather than an exclusive display-mode switch. It can be obscured by other applications.
- Fullscreen: explicit opt-in to the legacy exclusive presentation path.

The native bridge forces windowed D3D8 device parameters in CreateDevice and Reset for both windowed modes. It uses the adapter desktop pixel format and resets the fullscreen-only refresh and presentation interval fields. The wrapper's AppControlledScreenMode is disabled so the game cannot override the selected mode.

The dgVoodoo output backend is explicitly Direct3D 11 feature level 11.0. The game still submits D3D8 commands, translated by dgVoodoo; its engine has not been rewritten for D3D11. A compatible dgVoodoo wrapper must be supplied with the imported game.

Windowed startup was checked on the supplied private game copy: a framed, non-topmost Football Mania window, D3D8 device reporting windowed=1, and unchanged desktop dimensions before and after startup. Evidence is local/windowed-verification.json; reproduce with python tools/verify_windowed.py after preparing the private copy and building the bridge. The verification script closes its own test process. Borderless configuration has automated coverage; an interactive borderless run is intentionally excluded from this test to avoid covering the owner's desktop.

## Video and HUD policy

Default presentation preserves the original aspect ratio. Opening logos should sit on matching black backgrounds; subsequent cinematics must retain their encoded aspect ratio, with integer scaling when it fits and aspect-preserving downscaling when it does not. Per-video integer scaling and adaptive HUD anchors are still implementation work. The experimental Hor+ switch is not evidence that these requirements have been completed.
