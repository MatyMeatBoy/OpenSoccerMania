# OpenSoccerMania work board

Updated: 2026-11-17. Current baseline: 0.1.0-pre.5. See
[scope and limits](releases/0.1.0-pre.5.md).

## Continuation verification, 2026-11-17

- Captured another real LOAD FAILED before any profile payload read, followed by
  a successful retry in the same process. External trace shows load phase 1,
  phase 2, then phase 14 with operation result 2 and status 0; expected/read bytes
  both remain zero. The seed save hashes remain unchanged. A later harness
  navigation error entered an empty-name prompt, then cancelled without creating
  a file; it occurs after the failed load. Evidence:
  `story-progress-assisted/summary.json`, `initial-load-events.json`, capture 63
  and command 9. This excludes that later prompt as the initial failure's cause.
- The original save worker has an independently reproduced lost-request race:
  it snapshots command zero under a lock, unlocks, then unconditionally clears
  the shared command under a second lock. A producer can publish in between.
  A signature-checked, stack-neutral x86 dispatch guard now sends only an idle
  snapshot straight to the existing yield continuation. Nonzero and invalid
  commands retain their original dispatch. The production-DLL fixture fails
  before correction (`save-worker-cb20lpyi`) and passes afterward; it models the
  publication order and verifies next-pass execution exactly once, registers,
  stack, dispatch boundaries and unsupported-code exclusion.
- All twenty-seven integration cases pass (`gpu-0zzw5vgq`), including separate
  signature-mismatch and unverified-executable controls. Python tests pass 125
  with one privilege skip. Independent review found no actionable introduced
  issues and inspected the emitted naked guard. The synthetic stop case reaches
  the yield continuation with a stopping flag; it does not execute the complete
  original worker exit. Real patched-process verification is ongoing. The
  observed LOAD FAILED is compatible with this race, but busy rejection ignored
  by callers and overlapping consumers are not yet excluded; universal load or
  Story progression acceptance remains open. See the
  [investigation plan](superpowers/plans/2026-11-17-save-worker-idle-race.md).
- Three fresh patched owner-game processes now show LOAD SUCCESSFUL, read all
  4800 payload bytes, leave save hashes unchanged and close with exit zero.
  The synchronous save-status logger is disabled; external read-only sampling
  and the engine's startup guard marker identify the tested path. Evidence:
  `save-worker-load-1/` through `save-worker-load-3/` summaries in the private
  evidence root. A fourth process also loads successfully and is exercising
  Story. These selected real loads and process closes support the correction;
  they do not prove all failure modes or instrument the full worker-exit body.
- Added an asset-free retained-programmable-shader Reset case. Its own shader
  uses the fallback projection epilogue; center-color and black-margin GPU
  readback verify both drawing and aspect correction before and after two
  successful Reset calls. This route already passes without a production
  change; the suspected lost-constant failure was not reproduced. All twenty-two
  integration cases pass (`gpu-vodp56a6`), with independent review finding no
  actionable introduced issues. This does not establish every shader/state path.
- A longer 1000-second Story probe reloads the configured team and observes
  two opponent goals with visible returns to kickoff. It closes cleanly with
  unchanged save hashes, but reaches the harness deadline before a final result.
  The global options screen was set to 4:00/power-ups off; effective Story-mode
  use of those preferences is not established. Evidence:
  `story-progress-short/summary.json` under the private evidence root. It does
  not accept a completed match or progression. A late command 41 is excluded;
  subsequent acceptance must allow for explicit replay-continuation waits.
- Projection state after Reset: the bare pinned backend restores its initial
  projection, but the bridge's GetTransform continued returning its cached
  pre-reset matrix. A new production-DLL assertion fails before correction
  (`gpu-mmg3ncmb`) and passes after invalidating that cache on successful Reset
  (`gpu-xt99a37_`, all twenty-one integration cases). Repeated logical sizes
  and adaptive/nonadaptive variants are covered. Bare-backend reference output
  is retained under `reset-transform-reference/` in the private evidence root.
  Independent review found no actionable introduced issues. This verifies a
  state-reporting defect, not the cause of the intermittent black replay.
- Window resize feedback is reproduced and corrected. An owner-game probe
  changes a client to 640x480, after which the backend expands it through
  1440x845 to 1540x845. Temporary, isolated SetWindowPos tracing identifies the
  backend DLL as the caller; those temporary hooks were removed from source.
  Adaptive source-ROI changes in Present now receive an explicit existing-client
  destination, preserving caller-provided destinations and exclusive fullscreen.
  An asset-free regression fails before the change (`gpu-3cap_coh`) and passes
  afterward, including widescreen and original-ratio variants. All twenty-one
  integration cases pass (`gpu-tzj35bum`); independent review found no actionable
  introduced issues.
- The corrected owner-game menu stays at 640x480; independent Windows client
  reads subsequently confirm 1280x540, 800x600 and restored 960x540. Native F1
  settings open/close correctly in the wide client. The process closes cleanly
  with save hashes unchanged. Evidence: `window-sizing-fixed/summary.json`,
  `window-receipts.json` and captures 1, 3, 4 and 7 under the private evidence
  root. This accepts the observed menu/settings resize path, not gameplay resize,
  minimize/restore, fullscreen loss or a real in-game Reset. The published pre.5
  bridge remains unchanged; this correction is unreleased.
- Adaptive-reset correction: a new production-DLL variant first fits a 640x480
  client into the physical 1920x1080 target, then repeats reset/recovery at the
  same client size. Before the fix, logical drawing invades the left margin
  (`gpu-46ekt26t`): `render_parameters` resets raster scales while the aspect
  cache skips recalculation because its fitted rectangle is unchanged. The
  raster mapping now invalidates that cache on creation/reset. All nineteen
  integration cases pass with the rebuilt bridge (`gpu-vadh0ba0`), including
  margin/content pixel checks across four logical resolutions. Python tests
  pass 125 with one privilege skip; independent review found no actionable
  introduced issues. First-frame behavior before Present and hardware loss
  remain outside this regression's acceptance scope. This is unreleased.
- Failed-reset/retry coverage now rejects null parameters and four invalid
  color-buffer resets, then recovers at 640x480, 1280x720, 1024x768 and 640x480.
  Each recovery checks logical target identity, viewport, actual physical-center
  pixel readback, presentation and final device release. All eighteen native
  integration runs pass (`build/tests/gpu-i08t_2st/report.json`). This closes
  explicit invalid-call recovery coverage, not hardware device removal, live
  resize or exclusive-fullscreen loss. No production change was needed.
- Refreshed the README's stale startup/backend/default-framing claims against
  current evidence, and documented the existing F1/controller settings entry.
- Published baseline `v0.1.0-pre.5` points to commit
  `548b37c8a2ed747ba8969e9d11fc6fe4c1e37838`. Source/portable ZIP inventories,
  SHA-256 sidecars and GitHub asset digests were checked; extracted source runs
  125 tests (two expected skips), extracted portable smoke passes, and public CI
  passes 125 tests with the private-owner-input skip. Release notes explicitly
  disclose intermittent Story load failure after an apparent successful autosave.
- The first Story team configuration persists: creating six players confirms
  AUTOSAVE SUCCESSFUL and changes SAVE0000.SAV. A later process initially shows
  LOAD FAILED, then a retry loads and reaches the configured City Park story map.
  Reload leaves the save hashes unchanged and closes cleanly. Evidence:
  `local/acceptance-2026-11-17/story-team-reload/summary.json`. This is team
  configuration persistence, not completed-mission progression or universal load
  acceptance.
- Post-release save-hook correction: a production-DLL regression sets a known
  Win32 LastError before the status accessor. The previous guard loses that
  value (`gpu-pr2gv40n`); an RAII restoration now preserves the original accessor's
  error state on every return. All seventeen integration cases pass after this
  correction (`gpu-hdixp0e4`). This change is unreleased and does not replace the
  published pre.5 binaries.
- Expanded post-release checks cover failed statuses and unrelated callers as
  well as normalization. All eighteen native integration cases pass, including
  a separate run with synchronous save-status tracing explicitly enabled
  (`gpu-l15i2ocv`); the Python suite passes 125 tests with one privilege skip.
  Independent review found no actionable introduced issues.
- Opt-in isolated SaveStatus events now retain repeated equal results, with
  timestamps and read/phase fields. Earlier per-caller suppression hid repeated
  operations; those traces cannot prove that no second read occurred. Synchronous
  diagnostic logging can influence timing, so it is not performance evidence.
- A separate A/B with that synchronous trace disabled in both variants and an
  external read-only observer loads the same Story save successfully with both
  published and corrected bridges. Each requests and receives 4800 payload bytes
  and completes the load phases without an operation error. Thus LastError
  preservation fixes a verified hook defect but is not yet proven to explain the
  intermittent frontend LOAD FAILED response. Evidence: `save-ab-legacy/` and
  `save-ab-fixed/` under `local/acceptance-2026-11-17/`.
- Four further fresh-process repeats with identical Story profile copies and
  synchronous status tracing disabled all show LOAD SUCCESSFUL, read the full
  4800-byte payload, leave save hashes unchanged and close cleanly. Evidence:
  `save-repeat-1/` through `save-repeat-4/` in the same private evidence root.
  These successful repeats do not reproduce or close the intermittent failure.
  Post-release commit `1086340` also passes public CI run `44140253665`.
- A 600-second isolated Story probe restores the saved team, enters City Park,
  observes an opponent goal and a visible return to kickoff, then closes cleanly
  with unchanged saves. Evidence: `story-mission-reload/summary.json`, especially
  captures 5, 34 and 35. It does not finish a winning mission or verify saved
  progression; the earlier intermittent black transition remains open across
  other replay/camera paths. No test game process remains after the probe.
- Preview alignment is now verified for the observed animated model path. The
  model renderer and its clear operation have separate, signature-checked caller
  scopes; the preceding UI flush is not marked as model geometry. Local preview
  cameras retain their original projection, model viewports use the centered UI
  area, and their logical depth-clear rectangles receive the matching mapping.
  Per-draw viewport restoration prevents repeated compression. XYZRHW and clip
  shaders that already receive UI compression are excluded, and the feature
  requires the verified main-camera hook to be available.
- Real private-desktop captures show all six candidates and all six selected
  players aligned with their cells, including the first/last models and confirm
  button on the lava background (`menu-preview-active/` commands 6 and 16).
  A second ice-background run confirms alignment; bounded traces show shaders
  31/39 are programmable, non-clip, non-flat and start each observed draw with
  the full 1920x1080 viewport before mapping to X=240, width=1440.
  Evidence: `menu-preview-shaders/summary.json` and `render-session.log`.
- The same process subsequently enters a real match (`menu-preview-active/`
  command 25). Presets 0/1/2 record vertical scale ratios about 1.0006, 0.8502
  and 0.7002 while source-camera position changes stay zero. Captures 26/27
  show the experimental views. The original setting was restored and appears
  in the final engine report; capture 28 was requested after the bounded process
  ended and is not verified. Both actual game runs close with exit zero.
- Current verification passes 21 named native CPU checks and seventeen fresh
  GPU/input/import cases (`build/tests/gpu-7n08vynf/report.json`). The rebuilt
  portable passes its four-page/import/graphics smoke
  (`local/acceptance-2026-11-17/portable-preview-smoke.json`). Preview mathematics,
  nested scope restoration, projection context selection and double-compression
  exclusions have native regressions. This does not certify every UI model asset,
  completed Story progression, physical controllers or all-map replay behavior.
- Save-list crash correction: the verified successful metadata consumer calls
  strlen on a nullable result and can scan beyond NUL when a record ends with
  control separators. A caller/signature-scoped status hook supplies safe list
  metadata for those cases; valid records, error statuses and other callers are
  unchanged. Files are never rewritten or quarantined by this correction.
- The production-DLL synthetic regression fails with the previous bridge
  (`gpu-g8tda0fi`) and passes with the guard. It covers null, blank, absent-team,
  unterminated and unreadable records, repeated polling of the normalized buffer,
  and rejection of unknown caller code. All seventeen current GPU/input/import
  runs pass (`build/tests/gpu-gljs8reb/report.json`).
  The rebuilt portable passes its four-page/import/graphics smoke
  (`portable-save-guard-smoke.json`). A final fresh integration run verifies the
  exact packaged bridge hash (`build/tests/gpu-c2g1bn_6/report.json`); rebuilding
  also changes PE timestamps, so prior full-file hashes alone are not reused.
- A/B owner-profile evidence is explicitly intermittent: the first profile copy
  containing the extra 64-byte placeholder crashes at 004F9CD8 while the copy
  without it loads; a repeat loads both. Thus the placeholder alone is not proven
  to determine the crash. With the guard, both real copies load successfully,
  reach Main Menu and close with exit zero, each recording one metadata
  normalization. All save filenames, sizes and SHA-256 hashes remain unchanged.
  Reports: `load-complete-guard/summary.json` and `load-partial-guard/summary.json`
  under `local/acceptance-2026-11-17/`. This verifies the identified unsafe list
  parsing and these profiles, not every possible save-load failure or persistent
  completion of Story missions.
- Reproduced a missing D3D8 Clear mapping: the game clears logical rectangles
  before drawing animated menu models, while the physical backbuffer is larger.
  A new asset-free production-DLL fixture fails before the correction because
  the physical center remains uncleared (`gpu-s5xatsg4`). Clear now maps rectangle
  edges using the same raster scales/offsets as viewports. Physical presentation
  bars call the original API; offscreen targets retain their original coordinates.
  Fifteen fresh GPU/input/import runs pass (`build/tests/gpu-fsblv560/report.json`),
  including color/depth/stencil, multiple rectangles, partial viewports and 4:3
  letterboxing. This does not by itself prove the earlier black replay is fixed.
  The rebuilt portable passes its four-page/import/graphics smoke with the new
  bridge (`local/acceptance-2026-11-17/portable-clear-smoke.json`).
- Real Story team selection now shows all six complete animated models and can
  select all six, confirm a team name and reach the City Park story map. Evidence:
  `local/acceptance-2026-11-17/menu-clear/` commands 6, 16, 17 and 18. Horizontal
  alignment of the models with the centered UI still needs correction. Earlier
  camera-only/context-only hypotheses were removed after they failed to explain
  map-dependent occlusion; the final change is the independently reproduced Clear
  mapping. Bounded camera call-site logging is available only in isolated probes.
- Story persistence remains open. A reused disposable profile later reported
  LOAD FAILED and contains a 64-byte SAVE0002.SAV created during incomplete empty
  slot navigation, while the earlier two full save-bank hashes remain unchanged.
  The complete profile is retained in `menu-camera-clean/failed-profile/` for
  investigation; its presence is not yet proven to cause the load failure. The
  visual regression was therefore repeated in a new private entry using Don't
  Save, which cannot establish persistent story progress.
- Completed a real Quick Start match on the ice field: City Footballers 0–1
  Leo's Lions, with goal/replay, halftime, second half, final statistics and return
  to Main Menu. The 562-second private-desktop run closed with exit zero. This
  supersedes the earlier absence of any completed match, not all-map acceptance.
  Evidence: `local/acceptance-2026-11-17/goal-trace/summary.json`, capture 520
  (final statistics) and `command-1.png` (Main Menu).
- Read-only tracing recorded 10,008 active-camera samples: aspect remained
  1.777777791, up-vector length remained one, and no nonfinite camera values were
  observed. Completed fader intervals lasted at most 1.125 seconds; the internal
  clock was never frozen in the sampled sequence. The earlier persistent black
  replay exit did not reproduce, so its cause and universal correction remain
  unproven. The latest engine-log block is saved separately to exclude earlier
  appended sessions; 81 positive report windows have median 49.9078 steps/s.
  Partial windows/catch-up remain visible; this is not timer/audio equivalence.
- A new process after that match recognizes the private `LEGO` profile, reports
  `LOAD SUCCESSFUL` and reaches Main Menu (`match-reload/` captures 48 and 55).
  That bounded reload process also closes with exit zero.
  Quick Start did not change the save-bank hashes during this match. This proves
  profile readability after a complete match, not persistent story/tournament
  progress or every save mode.
- Private diagnostic input is now exclusive: a production-DLL regression injects
  a fake physical Space key and proves it cannot enter the diagnostic keyboard.
  Incomplete mailbox packets stay neutral; ordinary input still preserves the
  physical key. The async-key settings path follows the same diagnostic ownership.
  Twenty native CPU checks and thirteen GPU/input/import cases pass with the
  updated bridges (`build/tests/gpu-9dcr08zf/report.json`). Python verification
  also passes: 124 tests, with one existing Windows symlink-permission skip.
- Corrected the interpretation of `689644/648`: disassembly shows ownership of
  loaded menu resources, not active pause state. A real private-desktop match
  retains those flags while its dialog manager is empty; Pause Menu makes that
  manager nonempty. Gameplay context now uses the verified manager layout rather
  than those resource flags. A repeated real match publishes gameplay true at
  105 seconds, false with visible Pause Menu at 115, and true after Resume at
  125, while thirteen actors remain loaded. Captures and assertions are retained
  in `local/acceptance-2026-11-17/ui-fixed/context-result.json`.
  The rebuilt portable passes its four-page/import/graphics smoke with these
  exact bridges (`local/acceptance-2026-11-17/portable-context-smoke.json`).
- One private UI navigation probe exited with access violation at `004F9CD8`,
  where the original executable reads a string pointer from `683750`. The next
  220-second controlled navigation loaded the profile, entered a match, opened
  Pause Menu and closed cleanly. This does not establish a fix for that one crash;
  pointer/provider tracing is needed if it reproduces.
- Private save API correction: executable import inspection showed the real game
  calls `SHGetSpecialFolderPathA` (CSIDL_APPDATA) and `RegCreateKeyExA`; earlier
  portability hooks covered different variants. Both actual calls now route to
  private storage/virtual keys, in early IAT setup and the runtime hook path.
  A fixture invoking the production DLL's actual imports fails with the previous
  bridge and passes with the correction; original API stubs deny machine access.
- The actual game now creates `profile/LEGO Media/Football Mania/Dev0/OPTIONS.SAV`
  and save banks. A second process recognizes the default `LEGO` profile, shows
  `LOAD SUCCESSFUL` and returns to Main Menu. Both bounded runs close cleanly.
  Evidence: `local/acceptance-2026-11-17/save-roundtrip/`, including captures 55,
  59 and 80. This verifies basic profile creation/reload, not completed-match
  progression or recovery of every save variant.
- Corrected diagnostic capture for the owner's RGB565 backbuffer. Previously it
  logged successful GPU readback but wrote no image for format 23. It now converts
  through the actual row pitch into 32-bit BMP, with color-value regressions and
  real game captures. Unknown formats and write failures no longer report success.
- A 390-second real private-desktop match probe now reproduces a goal (0:1),
  celebration and replay. After private Enter leaves the replay, a circular wipe
  is visible at about 341 seconds; the 370-second capture is almost entirely
  black while simulation/pose counters continue. This is an open transition
  failure, not full-match acceptance. The bounded close still exits zero.
  Evidence: `local/acceptance-2026-11-17/full-match/`, especially capture 340,
  `after-continue.png`, capture 370 and `after-goal-gameplay.png`. Next work is
  tracing the iris draw/state and comparing controlled timing paths before any
  correction; the unchanged camera-position counter alone does not explain it.
- Save/capture continuation verification: twenty native CPU checks pass, and all
  thirteen fresh GPU/input/actual-import cases pass (`build/tests/gpu-1cyyzyim/`).
  The portable was rebuilt with these changes; its four-page/import/graphics
  smoke passes (`local/acceptance-2026-11-17/portable-save-api-smoke.json`).
- Isolation correction: CPython ignores an arbitrary `lpDesktop` attribute on
  `subprocess.STARTUPINFO`. A child-side check reproduced `Default` with the old
  harness. Those earlier runs establish background execution and unchanged input
  desktop only, not a separate desktop. The harness now calls CreateProcessW
  with a real native STARTUPINFO.lpDesktop; the child confirms its actual unique
  test-desktop name. Nonzero exit/output capture and bounded termination have
  integration checks. The nineteen native CPU checks and twelve GPU/input runs
  pass after this correction. Earlier historical isolation claims below must be
  read with this limitation until their particular probes are repeated.
- The normal frozen launcher now has a repeated, directly observed startup and
  clean close on the actual private desktop with a nonadministrator token and
  real NTFS install-folder write denial. It creates logs/library in the user
  data fallback. Report: `local/acceptance-2026-11-17/frozen-standard-user-result.json`.
- Repeated the owned game's 4K startup with the corrected desktop harness and
  distributed backend: the process token is not administrator, 3840x2160 targets
  were allocated, and the 20-second probe closed with exit code zero when its
  actual private-desktop window received the close request. Evidence:
  `local/acceptance-2026-11-17/4k-private-result.json`. This closes this bounded
  startup/shutdown case, not full-match/save-load or every game's exit path.
- Original shared branding now covers launcher windows/dialogs, the frozen EXE
  and game-window small/large icons (including fullscreen). SVG, multi-size ICO,
  16/32px favicons, 256px PNG and Apple touch PNG live under `assets/branding/`.
  The native icon regression failed without the resource and passes with it;
  the game executable's hash/resources remain untouched. Source/portable
  allowlists include only the exact original project assets.
- Final local candidate: 124 Python tests run with one existing symlink-privilege
  skip; nineteen native CPU checks and twelve GPU/input cases pass. Frozen smoke
  passes all four pages, synthetic ISO import, graphics save and branded resource
  validation. Normal frozen startup under denied install writes also verifies
  both small/large window icons and clean close. Report:
  `local/acceptance-2026-11-17/portable-branding-smoke.json`. The candidate is
  `build/portable/OpenSoccerMania/`; no new release has been published.
- Standard-user operation: launcher startup now selects writable portable data
  or `%LOCALAPPDATA%\OpenSoccerMania`, testing actual file and directory creation
  in the data root, logs and library. Read-only existing preference/log files also
  trigger fallback. Source originals and protected installations stay unchanged.
  Fresh private runtime/configuration copies drop the inherited read-only bit.
- The actual session token is not administrator. Two isolated source-launcher
  probes with a real NTFS write denial on copied install folders passed automatic
  fallback, synthetic ISO import, video save, preferences and logs. Evidence:
  `local/acceptance-2026-11-17/standard-user-result.json`. ACLs were restored on
  the disposable fixtures afterward. The frozen launcher manifest already uses
  `asInvoker`; supported game startup bypasses the old registry-located helper.
  A native regression checks private registry set/query and profile writes while
  machine-registry fallbacks return access denied.
- A 510-second background game process reached active play using a private kickoff
  pulse. Switching the requested probe cap from 60 to 240 produced median
  sampled rendering of 58.696 and 99.545 FPS, with median logic rates 49.907 and
  49.8687 steps/s. Trial ranges include stalls/catch-up; these are not matched-scene
  performance gains, fixed 240 FPS or full timer/audio equivalence. Original
  camera-position-change counters stayed zero. No match completion/save/reload
  was observed and shutdown still required termination. Raw evidence and summary:
  `local/acceptance-2026-11-17/match/`. SDL enumerated no physical controllers.
- Rebuilt both native bridges; 124 Python tests run, with the existing Windows
  symlink-permission skip. Nineteen native CPU checks pass, plus late SDL
  discovery and actual input-bridge factory checks.
- Added the opt-in `tools/test_native.ps1 -Gpu` command so existing GPU
  regressions no longer depend on untracked owner-specific test directories.
  Every run creates new asset-free fixtures and uses the hash-pinned bundled
  dgVoodoo backend. Reports identify the exact bridge/backend hashes. The cases
  cover buffered/immediate sky draws, signed-RHW horizon, far-depth boundary,
  color-only backgrounds, stencil/HUD separation, native settings, menu and
  title artwork, layered COM/window ownership and the private input mailbox.
- Fresh fixtures exposed an old COM test assumption: dgVoodoo 2.87.6 rebuilds
  its shared device table on a second CreateDevice. The regression now restores
  the known shim only when the slot still points at the exact expected bridge,
  then checks nonrecursive release and complete cleanup of both devices.
- Reproduced a graphics reset bug: GetRenderTarget/GetDesc returned physical
  1920x1080 after Reset instead of the game's logical 640x480. The successful
  reset path now rebinds backbuffer identity, marks the default target active
  and restores the full logical viewport. The new GPU regression failed before
  the correction and passed afterward; repeated logical resolutions are checked
  separately from real movie/resize acceptance.
- A background owner-game startup on the RTX 4060 Laptop GPU with the pinned
  distributed backend allocated 3840x2160 color/depth targets, including 4x MSAA.
  Evidence: `local/acceptance-2026-11-17/4k-bundled-result.json` and matching
  backend log. The 20-second startup probe required forced shutdown; it proves
  allocation only, not a complete match, 4K visual quality, VRR or clean exit.
- Reconciled existing private `local/menu-evidence/menu-evidence.json`: loading,
  footer and pause captures already exist. This closes those specific captures,
  not all pause/loading variants or physical-controller acceptance. The probes
  did not establish universally clean shutdown.
- Rebuilt the portable candidate with the reset correction; isolated frozen
  smoke passed all four pages, synthetic ISO import, bundled dependency hashes,
  temporary settings and graphics provisioning. Local report:
  `local/acceptance-2026-11-17/portable-smoke.json`. This is a local candidate,
  not a new published release. Independent code review found no actionable
  introduced issues; failed Reset/retry remains a separate coverage gap.
- Remaining priorities stay goal/replay camera transitions, all-map world
  effects, complete match/save/reload, device reset/resize/movie transitions,
  physical controllers, real image layouts, 4K/VRR and simulation-connected
  online play. Passing synthetic checks does not close these acceptance items.

## Clean-PC packaging correction

- Pre.2 omitted the backend and default configuration, incorrectly relying on
  a preexisting private-library donor. The new pack includes exact pinned vendor
  components for this dedicated game and provisions them automatically.
- The default image reader uses no installed tools. Empty-library ISO import
  regression tests disable Windows mounting and forbid external processes.
- Frozen smoke coverage now includes a synthetic ISO import and graphics profile
  save, rather than only opening pages. Original game assets are not in fixtures.
- Vendor binary/configuration bytes are preserved with Git binary attributes and
  PyInstaller's no-UPX build, with source/portable hash and license checks.

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

### Unreleased integration checks

- Pre.2 checks: 76 Python tests run, with one platform privilege skip. The native
  suite has 14 passing checks, including title BMP validation, hook-chain
  ownership and precise stencil-composite classification.
- An asset-free fixture using the production bridge reproduces the stencil
  resolve being limited to 4:3. The corrected bridge shades first/last pixels
  across 1920x1080 and preserves an ordinary same-depth HUD control. This is one
  identified world-effect path, not all-map acceptance.
- The Windows x64 portable candidate opens/closes all four launcher pages in an
  isolated desktop, validates bundled resource hashes/architectures and leaves
  settings temporary. Closed ZIP extraction and release identity checks apply to
  the final artifacts separately.
- Native F1 settings and Start + View/Select transport are implemented. The
  settings overlay has an isolated GPU check; physical controller acceptance
  and original in-game button-art replacement remain open.
- Both regional titles decode privately. Generated blue sidebars preserve the
  original center and match private edge colors; synthetic GPU checks verify
  exact center boundaries and restored graphics state. Real title/replay/wipe
  acceptance remains separate. See [title branding](title-branding.md).
- Broader image intake, damaged-executable donor repair and private backend
  reuse have bounded parser/worker tests. Real images of each declared layout
  remain acceptance work; extension alone never establishes support.
- A reproduced layered COM Release recursion now has an old-fail/new-pass GPU
  regression. See [lifecycle evidence](com-hook-lifecycle.md).

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

### Replay sky and current priorities

- Reporter confirms that skies and animations look substantially better, while identifying remaining goal-net/shadow clipping and a bad camera transition after goals. Original apparent zoom must remain the default; distant zooms belong in experimental settings.
- A live pirate-map sky trace crossed the horizon with signed RHW. GPU tests showed that depth normalization alone still clips negative W. A signed projected-texture path now preserves raster coverage and texture perspective; GPU regressions pass through buffered and immediate indexed/non-indexed paths.
- A second low-angle trace crossed the far-depth boundary (`0.9998924`–`1.0001178`). Its black-output regression fails before the widened background predicate and passes afterward. Color-only backgrounds also have a reproduced/passing GPU regression.
- Private input-desktop state stayed unchanged throughout testing. Two bounded match probes exited cleanly after their own close requests; other probe states still require forced termination, so clean shutdown is not universally accepted.
- Next scope is recorded in [modernization follow-up](modernization-next.md): original-zoom camera/goal transitions, world-effect framing, F1 / Start+Select internal settings, controller prompts, broader image intake and selectable Soccer/Football title branding.

- Measure a controlled match with VSync off and distinguish CPU/render/presentation waits. Do not publish gains from different matches or changed quality settings.
- Compare real match timer, motion, power-ups and audio at multiple render caps with the original scheduler.
- Validate resize/maximize, 4:3, 16:9, ultrawide and movie transitions; exercise device reset and clean shutdown.
- Verify full match/save/reload, physical controllers, ISO import, 4K render-target allocation and VRR on actual hardware.
- Complete controller prompt mapping, launcher visual QA, packaging and English documentation refresh. VRS and temporal AA remain unimplemented.

This board distinguishes implementation from acceptance; it is not a claim that the complete engine has been fixed or recompiled.

### Pre.4 controller and import pass

- Read-only private executable replacement and interrupted pre.3 patch recovery now pass Windows fault-injection tests and disposable owner-file checks. Original files remain unchanged; no elevation is requested.
- Select / View releases a video-skip pulse only while the native movie state is active. Start remains pause; Start + Select consumes both standalone actions and opens settings. Python gesture tests and the native foreground/movie gate pass. Physical-controller movie acceptance remains pending.
- Four separate SDL-to-DirectInput gamepad slots replace gameplay keyboard emulation. The original input manager accepts up to sixteen pads, so four fit without changing that limit. Separate COM device-state tests pass; actual four-player assignment remains an acceptance item.
- Removed the permanent controller legend and Smooth Mode option. Legacy saved Smooth Mode preferences are forced off. Widescreen defaults on; original-ratio mode now has explicit physical presentation mapping and letterboxing math tests.
- Controller cursor ownership has native checks: actual mouse movement restores it; background windows and pointer positions outside the game client cannot hide it.
- Menu grid extension and original numeric-button replacement have synthetic GPU controls; private loading, footer and pause captures are recorded in `local/menu-evidence/menu-evidence.json`. Other variants and physical-controller acceptance remain open.
- A separately documented server/client foundation supplies four-seat, two-per-team rooms, ready/start barriers and bounded frame-tagged input relay. Six loopback tests pass. Engine synchronization, playable online matches and WAN latency remain pending; the launcher does not advertise online gameplay as ready.
- Current aggregate verification: 113 Python checks pass with one Windows symlink-permission skip; fifteen native checks pass. These counts do not establish physical-controller, online-gameplay or all-map acceptance.
