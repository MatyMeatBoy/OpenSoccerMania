# OpenSoccerMania work board

Updated: 2026-11-16. Preparing 0.1.0-pre.1 as a limited public source snapshot; see the release scope.

## Current pass: original clock and independent rendering

OptiPort reference: `MatyMeatBoy/OptiPort`, main commit `9673006504bf6712b320da70965bc3f65f2dd41a`, rechecked against upstream on this pass. Original dumps remain untouched. Runtime tests use `local/library/football-mania`; raw evidence stays under ignored `local/timing-evidence/`.

| Work | Status and evidence |
|---|---|
| Identify original rate selection | Implemented analysis: the common x86 routine selects 30, 37.5 or 50 according to hardware flags. No original 60/59.94 route established. |
| Compare region selection | English selected the Football Mania key; American selected the Soccer Mania key and Soccer title assets. Both reported a configured 50 Hz clock. This tests two modes of the supplied executable, not a separate intact American executable. |
| Measure real simulation | Native step instrumentation observed approximately 50 steps/s in both modes while rendering above 50 FPS. Preserve changing game-speed effects; do not overwrite the authoritative rate. |
| Independent render wait | Implemented for hash-verified executables with matching function signatures. Startup, gameplay and clean shutdown exercised. Partial hook installation rolls back. Long-session/audio/match-timer equivalence remains pending. |
| Render limits | Added settings for 60–480 FPS and Unlimited, separate from fullscreen refresh. Actual FPS depends on presentation and workload. VSync-off measurement in progress. |
| Camera/frustum | Camera builder receives the output aspect before constructing projection and CPU frustum. Live 16:9 aspect observed as 1.77778. Edge visibility and multiple aspect ratios still need regression checks. |
| Automated checks | 22 Python tests pass, including unknown-version engine-hook exclusion and adaptive window configuration. Native build succeeds. |
| Borderless hotkeys | F11 and Alt+Enter added, with foreground-only physical-key polling and held-key suppression. Automated F11 message tests passed in both starting modes, including return to previous window placement and clean exit. Desktop stayed 1536×864; internal target stayed 1920×1080. The backend replaces its window procedure during style changes; the bridge now reinstalls its subclass after that transition. |

## Remaining acceptance work

### Current render pass

- Confirmed and fixed sky XYZRHW coordinates remaining at 640 pixels on a 1920-pixel target.
- Reproduced a second black-sky mechanism with the captured depth/rhw values under hardware vertex processing: depth beyond the far clip plane plus scene fog. Background-only depth normalization and temporary fog suppression pass GPU readback tests; scene fog is restored afterward. A real isolated space-map menu now renders its nebula and stars across the output. The user's running build was not hot-reloaded.
- Replaced readback of WRITEONLY vertex/index buffers with bounded CPU mirrors captured during writes (32 MiB total budget). The earlier trace recorded ~3.3 ms/frame in screen vertex-buffer reads; isolated game traces now record zero such reads and mirror hits. This is not a claim of an equivalent total-FPS gain.
- Added shader/constant caches with invalidation for state blocks, reset and shader deletion. A six-run synthetic comparison measured medians of 3.4511 versus 3.1293 microseconds/draw, with overlapping trial ranges. It does not establish a gameplay FPS percentage.
- Added isolated Win32-desktop tests, per-test-process audio muting and private keyboard-state injection. No desktop switch, foreground activation or desktop key synthesis is used by this harness.
- Passed 22 Python tests, seven native math/ownership/input checks, and hardware-rendered indexed/non-indexed/repeating-texture tests, including fog and binding restoration.
- New reporter issue: some character animations appear too fast and the goalkeeper shakes at high render rates. Animation-rate/interpolation investigation is open; do not claim 240-FPS gameplay timing acceptance yet.

### Animation boundary correction

- Instrumented the verified animation-advance and pose-evaluation routines: 13 actors each advanced/evaluated once per simulation step, rather than once per rendered frame. The sampler now reads SA states directly and checks executable hashes.
- Identified the original interpolator's 0.05 boundary threshold, which substitutes coefficient 1 and can rewind the pose on the following render frame. The corrected path preserves the actual coefficient, including zero; it leaves simulation advancement unchanged.
- Same-process isolated A/B at a 240-FPS requested cap: correction disabled produced 96 rewinds/1800 frames and 26/600; enabled produced 0/1500 and 0/300, correcting 76 and 13 boundary frames. These counters measure interpolation continuity, not a locked-FPS performance result. Logic remained near 50 steps/s.
- Eight native checks pass, including endpoint continuity, tick-boundary progression, differently sized snapshot records and unchanged input banks. Python checks and native bridge build pass. Full visual acceptance of all animation blends/effects remains pending. See [animation investigation](animation-interpolation.md).
- GitHub's first CI run exposed a test-path alias mismatch (`RUNNER~1` versus `runneradmin`), not a discovery failure. The installer-tree assertion now compares canonical paths on both sides.

- Measure a controlled match with VSync off and distinguish CPU/render/presentation waits. Do not publish gains from different matches or changed quality settings.
- Compare real match timer, motion, power-ups and audio at multiple render caps with the original scheduler.
- Validate resize/maximize, 4:3, 16:9, ultrawide and movie transitions; exercise device reset and clean shutdown.
- Verify full match/save/reload, physical controllers, ISO import, 4K render-target allocation and VRR on actual hardware.
- Complete controller prompt mapping, launcher visual QA, packaging and English documentation refresh. VRS and temporal AA remain unimplemented.

This board distinguishes implementation from acceptance; it is not a claim that the complete engine has been fixed or recompiled.
