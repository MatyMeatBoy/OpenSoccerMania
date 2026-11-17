# Display modes and Direct3D 11

OpenSoccerMania defaults to **Windowed**. Graphics offers Windowed, Borderless and Fullscreen.

- Windowed: a normal framed window, initially bounded to the desktop work area. The mouse is not captured by the wrapper and the window is not always on top.
- Borderless: a desktop-sized window without borders, using windowed presentation rather than an exclusive display-mode switch. It can be obscured by other applications.
- Fullscreen: explicit opt-in to the legacy exclusive presentation path.

The native bridge forces windowed D3D8 device parameters in CreateDevice and Reset for both windowed modes. It uses the adapter desktop pixel format and resets the fullscreen-only refresh and presentation interval fields. The wrapper's AppControlledScreenMode is disabled so the game cannot override the selected mode.

The dgVoodoo output backend is explicitly Direct3D 11 feature level 11.0. The game still submits D3D8 commands, translated by dgVoodoo; its engine has not been rewritten for D3D11. The portable package includes the pinned compatible dgVoodoo backend; importing a valid game does not require a separate graphics download.

Windowed startup was checked on the supplied private game copy: a framed, non-topmost Football Mania window, D3D8 device reporting windowed=1, and unchanged desktop dimensions before and after startup. Evidence is local/windowed-verification.json; reproduce with python tools/verify_windowed.py after preparing the private copy and building the bridge. The verification script closes its own test process. Borderless configuration has automated coverage; an interactive borderless run is intentionally excluded from this test to avoid covering the owner's desktop.

Post-pre.5 source builds also preserve manual window sizes while adapting the
source presentation rectangle. The bridge supplies an explicit client destination
to the backend: changing the source ROI must not resize the application window.
An isolated owner-game menu/settings probe retains 640x480, 800x600 and 1280x540
clients and returns to 960x540 with a clean close. Source-only GPU regressions
cover both widescreen and original-ratio presentation. Evidence and exact limits
are recorded in [the work board](WORKBOARD.md); gameplay resize, minimize/restore
and exclusive device-loss acceptance remain open.

## Video and HUD policy

Widescreen presentation defaults on. Disabling it fits the original 4:3 content within the output, preserving proportions with side or top/bottom bars. Opening logos should sit on matching black backgrounds; subsequent cinematics must retain their encoded aspect ratio, with integer scaling when it fits and aspect-preserving downscaling when it does not. Per-video integer scaling and adaptive HUD anchors are still implementation work. The experimental Hor+ switch is not evidence that these requirements have been completed.
