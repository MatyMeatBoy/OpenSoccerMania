# Modernization follow-up

User direction: continue through the pending work without interrupting the input desktop. All source, UI and documentation remain English. Source dumps and imported assets remain private.

## Priority order and acceptance

1. Finish the signed-RHW sky/horizon correction and validate real replay/low-angle scenes. Keep GPU readback regressions and avoid changing world fog or texture perspective globally.
2. Correct camera transitions after goals and preserve the original apparent zoom by default while extending horizontal visibility. Investigate the actual projection builder and camera ownership, rather than assuming that FOV expansion alone preserves composition.
3. Correct remaining shadow/effect clipping at the original 4:3 boundaries, including goal-net shadows. Distinguish world layers from HUD composition.
4. Add shared launcher/internal settings for original framing and optional distant experimental zooms. Internal settings open with F1 or controller Start + Select/View. Navigation and prompts use Xbox conventions and support SDL-mapped controllers plus raw-device fallback. Chord handling must avoid also sending pause/back commands to the game.
5. Broaden read-only disc-image intake with explicit supported-format detection and clear diagnostics for unsupported/damaged inputs. The three owner sources must be selectable; the zero-filled executable cannot be treated as runnable. Unknown executable versions need verified compatibility evidence before enabling binary hooks.
6. Inventory owner-provided Soccer Mania / Football Mania title assets and offer a launcher branding choice. Do not distribute extracted logos or game data in source or releases.
7. Continue frame-pacing/resource profiling and full-match/save-load/controller/device-reset acceptance. Keep simulation at the verified original clock, distinguish requested caps from measured FPS, and retain logs.

## Internal settings design

Use the native bridge's render overlay so the panel stays inside the game. Reuse the launcher's video configuration model for camera/overlay/controller preferences. Keep desktop-level display changes separate from immediately applicable camera/overlay changes; mark any option requiring restart. F1 and Start + Select/View toggle the panel. D-pad/left stick move, A confirms, B backs out, LB/RB change sections. While the panel is open, consume menu navigation for the panel and release game-input keys. Restore normal input on close, focus loss and controller removal.

Default camera framing preserves original vertical scale, distance and target behavior. Distant views are explicitly experimental presets and are never selected automatically after a goal or replay. Persist the choice without modifying authoritative gameplay speed.
