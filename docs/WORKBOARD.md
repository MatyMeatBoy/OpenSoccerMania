# OpenSoccerMania work board

Updated: 2026-11-17. Current published baseline: 0.1.0-pre.8. See
[pre.8 scope and limits](releases/0.1.0-pre.8.md).

## Catalog submissions and public visibility, 2026-11-17

- The user requests Quiver Launcher and recomplauncher.com listings under
  MatyMeatBoy, extended to the user's game/launcher projects. Both public catalogs
  already contain ReKHCoM, ShiningSource and NewSnapRecomp. Both verified developer
  profiles use the current classic Meat Boy GitHub avatar; do not duplicate those
  entries or replace the profile picture unnecessarily.
- Made [SaturnTaisenFirst](https://github.com/MatyMeatBoy/SaturnTaisenFirst) public
  after checking its tracked history for private inputs/credential candidates.
  Updated its public description and README visibility wording; preserved the
  active local worktree's uncommitted code. Created and pushed public
  [DynaBrothers1-2](https://github.com/MatyMeatBoy/DynaBrothers1-2) from audited
  committed source `c6e849edf66ff7e835c3e673d2b6fb7d0e5970e0`, retaining its
  `work/dynabrothers` branch and unrelated pending local changes.
- Published reviewable catalog cards on separate `codex/catalog-listing` branches:
  [OpenSoccerMania](https://github.com/MatyMeatBoy/OpenSoccerMania/blob/codex/catalog-listing/docs/catalog/README.md),
  [Sakura Wars](https://github.com/MatyMeatBoy/SaturnTaisenFirst/blob/codex/catalog-listing/docs/catalog/README.md)
  and [Dyna Brothers](https://github.com/MatyMeatBoy/DynaBrothers1-2/blob/codex/catalog-listing/docs/catalog/README.md).
  Each has bounded descriptions, requirements, credits, an original/code-native
  project icon, a real unaltered runtime capture and a media manifest. All five
  files per card match their public downloads. OpenSoccerMania is explicitly a
  PC compatibility/mod launcher, not a console recompilation; Dyna Brothers is
  source development without a published standalone ZIP at this point.
- Additional public-release drafts cover ShiningSoul2Recomp and
  Ultimate2C-FixDuplicates. The latter is a hardware utility whose repair/restore
  requires user-approved elevation; it must not be advertised as an automatic
  game patch or applied silently. Forks need distinct-project/release review and
  upstream credits. Research libraries and unrelated software are not represented
  as playable native games. The full inventory stays private under
  `local/catalog-submissions/submission-inventory.json`.
- **No new catalog submissions have been sent.** Both portals require authenticated
  account access. The Chrome/computer-use runtime fails before connecting
  (`failed to write kernel assets`, missing path), including reset/retry. Do not
  bypass authentication, claim a pending entry is accepted, or duplicate the
  three existing entries. Public cards and local evidence are ready for the
  authenticated portal pass when access is restored. Contact details are not
  included in public cards. Prior accepted release artifacts remain unchanged.

## Controller-family baseline, 2026-11-17

- Published [pre.8](https://github.com/MatyMeatBoy/OpenSoccerMania/releases/tag/v0.1.0-pre.8)
  from exact source/tag target `899fd96a84d2efe4528ecc30621f22760b8d9333`.
  CI run 44197330911 succeeds. Source ZIP has 306 inventoried members and SHA-256
  `bdb4798f05f7cb3fe3af3e78b16577f5d0bb1fe34c3b5a8d19165449475d16ac`;
  Windows ZIP has 1089 members and SHA-256
  `5e373c781c665442e1be3fcc2ef4b37e4770eef2d02e0bf4e5c72d345bc36371`.
  Extracted source passes 148 Python tests (two environment/owner-data skips);
  extracted frozen launcher passes all four preview/import/resource checks.
  Both inventories and all four public downloads match their recorded hashes.
  Earlier pre.7 tag still resolves to `7dbdec65f5e7543403a124a4f1f44e92d6965651`.

- Implemented Xbox 360, PlayStation 4, Switch Pro and Steam Deck visual families
  with SDL type detection, Auto/manual style, positional labels and licensed CC0
  assets. The original menus and F1 use the embedded atlas; launcher Controls
  previews all four layouts. Physical Nintendo south/east remain B/A.
- Four-family launcher UI/image/label/persistence QA and real-game F1/menu
  captures pass on private desktops; all 16 commands complete and the game exits
  cleanly. Evidence: `local/controller-assets/qa/`,
  `local/controller-assets/game-qa/`. These are simulated input/display tests,
  not physical PS4/Switch/Deck or Linux/Proton hardware acceptance.
- The pre.8 frozen candidate passes four-page/import/graphics smoke with all four
  previews. A real standard token starts it with NTFS install writes denied,
  retains small/large icons, creates writable user storage and closes cleanly;
  test ACLs are restored. `local/release-verification/pre8-build/` records these
  checks. Final source passes 148 Python tests (one privilege skip), 22 named
  native CPU checks and 31 GPU/input/import cases using the exact frozen DLL.
  Final archive/publication checks are recorded above.
- OptiPort's reusable PC-modding, presentation-clock, licensed-controller-asset
  and logical-state audit methods are integrated in
  [OptiPort PR27](https://github.com/MatyMeatBoy/OptiPort/pull/27), canonical main
  `58843a2c9122720f5db8649c0d9cfdb60dc479cf`. All 36 installed canonical files
  match that revision; an earlier installed copy is backed up privately.
  This updates reusable procedures and evidence boundaries, not model weights.

## Continuation verification, 2026-11-17

- Identified concrete writers for the two differing conditional bytes: the
  initializer copies the low byte of an address argument, while a reset copies
  saved incoming ECX. A new phase-verified Friendly run captures matching
  initializer-argument/serialized low bytes, the full 18-component first state,
  and clean shutdown with copied saves/input desktop unchanged. The reset did
  not execute in that run, initializer-entry flags were not logged, and logical
  use/unusedness remains unproven. No bytes are masked. See
  [conditional-byte provenance](network-initial-state.md#follow-up-conditional-byte-writer-provenance).

- A second controlled Friendly pair captures flag 1 around all component calls:
  the 16-byte conditional serializer really copies in this pair, so its remaining
  changed words cannot simply be called padding. All 130 corresponding owned
  actor buffers (22932 bytes per process) match; complete resource/state coverage
  and independent RNG remain open. Both probes close cleanly with saves/input
  desktop unchanged. See [owned-buffer follow-up](network-initial-state.md#follow-up-owned-buffers-and-conditional-flag).
- Latest user priority adds glossy Xbox 360-style prompts plus PS4 and Switch Pro
  families, with matching controller-scheme labels and compatibility. Asset selection, licensing and integration are recorded above; existing broader game/network requirements
  remain active.
- Resolved the remaining 26 changed actor words and 19 global/team words through
  descriptor copies, team-array references and dereferenced player contexts.
  All 214 words sharing the allocation-like delta now have reference provenance;
  captured descriptor/context aliases also verify. This is not a complete pointer
  schema or stable-ID/content equivalence, and no bytes are masked. Conditional
  writes and inline RNG remain unresolved. See [remaining references](network-initial-state.md#follow-up-remaining-reference-semantics)
  and private `remaining-reference-provenance.json`. No production input, seeds,
  timing or published artifact changed.
- Traced changed actor words to constructor/cleanup/use-site reference semantics:
  13 field offsets per actor, accounting for 169 of 195 changed actor words in
  the controlled pair, are resource references or owned allocator buffers.
  Two changed actor fields plus global/team reference candidates still need
  provenance; stable IDs, referent contents and a complete pointer schema are
  not established. No bytes are masked and no logical equality is accepted.
  See [reference provenance](network-initial-state.md#follow-up-changed-actor-reference-provenance)
  and `local/netplay-initial-2026-11-17/actor-reference-provenance.json`.
- Captured the first original bucket-0 state in two fresh, phase-verified Friendly
  setups: LEGO All Stars/Lawbringers, Western Junction, identical profile/options
  copies and four pads. All 18 component spans concatenate exactly to 28604 bytes
  at frame zero, seed/shared RNG 999. Four components differ in 560 raw bytes;
  214 changed dwords share an allocation-like delta, while two conditional-write
  words and the separate inline RNG remain unresolved. No unexplained difference
  is masked or accepted as logical equality. Both processes close cleanly with
  save hashes/input desktop unchanged; a wrong-menu exploratory run is excluded.
  See [initial-state comparison](network-initial-state.md) and private
  `local/netplay-initial-2026-11-17/comparison.json`. Production/release unchanged;
  typed state equivalence and playable synchronization remain open.
- Resolved the logical registry lifecycle: the empty head was consumed staging
  metadata. All 24 captured registrations match three persistent lists containing
  18/1/5 nodes. Bounded real-game traces record 96 save-dispatch and 96 control-save
  returns, including the six-control 74-byte payload with a 76-byte return stride.
  No restore event is observed. Fresh control flow also identifies an inner
  per-step control refresh before recording, so edge survival to the clock marker
  alone is not acceptance of actor-consumed input. Clean close, unchanged input
  desktop and copied save hashes verify. See [logical serialization](network-replay-state.md)
  and `local/netplay-registry-2026-11-17/summary.json`. Identical initial-state,
  complete logical coverage, input gating and playable online remain open.
- Demand-triggered post-kickoff tracing verifies 1024 complete update/save
  sequences; 245 show RNG use after common return inside the mode dispatcher.
  The active interpolation chain has 16 nodes totaling 6211 registered bytes;
  copied process pointers and three bytes outside the final callback's writes
  make a whole-bank hash unsuitable as an authoritative cross-process checksum.
  Compact control serialization belongs to a different registry, observed empty
  in this run; earlier callback documentation now distinguishes capability from
  active restoration. Clean close, input desktop and three save hashes verify.
  See [active state ownership](network-engine-boundary.md#active-play-and-separate-snapshot-registries)
  and `local/netplay-state-2026-11-17/summary.json`. Full logical-state coverage,
  identical setup and playable synchronization remain open.
- A private completion probe records 384 complete marker/mode/common/return/save
  sequences on one thread. All sampled calls use one dispatcher with no skipped
  update or early return. The first iteration resets its raw tick from 1 to 0
  before mode entry, and one mode body changes RNG after the common return.
  The late window covers introduction/kickoff transition, not moving-play
  determinism. Clean close, unchanged input desktop and all three save hashes
  verify. See [completion trace](network-engine-boundary.md#bounded-completion-trace)
  and `local/netplay-completion-2026-11-17/summary.json`. Production binaries and
  the pre.7 release remain unchanged; complete state equivalence is still open.
- Refined the network update boundary against freshly disassembled owner bytes:
  a clock iteration can skip both mode calls, and either dispatcher can return
  before its common update. Both complete additional work afterward; the outer
  loop then copies seven words per counted record before snapshot saving. All
  24 selected instruction anchors match the supported executable. The next
  runtime probe must distinguish these paths before assigning network frames;
  interpolation serialization is not yet a complete authoritative checksum.
  See [mode completion](network-engine-boundary.md#mode-completion-and-skipped-updates).
  No production hook, input, timing or published artifact changed.
- Published [pre.7](https://github.com/MatyMeatBoy/OpenSoccerMania/releases/tag/v0.1.0-pre.7)
  from exact commit `7dbdec65f5e7543403a124a4f1f44e92d6965651`; CI `44178977744`
  passes. Root verification runs 135 Python tests (one privilege skip), 21 native
  CPU checks and 28 exact-DLL GPU/input/import cases (`gpu-4ajah0wk`). Extracted
  source runs 135 tests with two separately confirmed skips: owner assets are
  excluded and symlink creation requires Windows permission. Both archive
  inventories/file hashes and all four public downloads/sidecars verify.
- The rebuilt and extracted pre.7 launcher passes four-page/import/graphics smoke.
  Normal startup with a nonadministrator token and actual protected-install write
  denial uses user storage, retains icons and closes cleanly; ACLs are restored.
  Both rebuilt native images match the owner-game four-pad accepted images after
  normalizing only PE timestamps/checksum. Pre.6 candidate, public assets and tag
  are preserved; its candidate is at `build/released-pre6/OpenSoccerMania/`.
  Private publication receipts are in `local/release-verification/pre7-build/`.
  UTF-8 defects in new pre.7 and current-tree pre.4 notes were corrected before
  final packaging; the discarded provisional source archive remains private.
  This release is a baseline, not acceptance of playable online synchronization
  or the remaining hardware/campaign/all-map requirements.
- Identified shared random-state and common-update seed ownership. Startup uses
  a timing seed; the common update reloads a counter initialized to 999, then
  increments it before returning to additional mode-specific work. A private
  source-copy trace verifies 256 BEGIN/seed/END triples across initial and active
  match windows: every seed load and counter increment matches. All 254 adjacent
  pairs show other shared-generator use between those calls, so the common
  routine's exit cannot be treated as the whole-step result. Separate inline
  and temporary random-state domains also exist. Clean close is exit zero,
  input desktop unchanged and copied save hashes match. Evidence:
  `local/netplay-rng-2026-11-17/summary.json`. See
  [random-state ownership](network-random-state.md). No production seeds/hooks
  change; identical initial state and cross-process determinism remain unproven.
- Mapped the raw DirectInput cache (`pad +0x2b8`, 272 bytes), normalization and
  gameplay control edge fields. Corrected the earlier generic-update attribution:
  the direct-pad wrapper is `0x42e440 -> 0x42e680`, with message-pump/menu callers;
  `0x410740` alone does not establish its ownership. Gameplay records refresh at
  `0x485560` and have separate compact save/restore callbacks retaining held,
  pressed, released and previous masks. In an active-match private pulse trace,
  all 1558 complete target-control/next-marker pairs retain those four masks;
  all 196 observed A press edges reach the next marker. No suspected edge loss
  is reproduced in this selected case. The probe closes cleanly with copied
  saves and input desktop unchanged. Evidence:
  `local/netplay-cache-2026-11-17/summary.json`. Logging covers one normalized
  target; it does not prove full four-pad equivalence or deterministic online
  simulation. See [cache ownership](network-input-cache.md). No production
  input/scheduler behavior changed.
- Inspected the engine/network boundary instead of routing relay frames through
  an assumed tick callback. `0x41fc70` only advances clock/snapshot bank; its sole
  inspected call is inside the pending-step loop, while a generic device update
  runs in the outer loop. The backedge bypasses that generic call; the refined
  direct-pad and normalized-control ownership is documented above. A private source-copy
  trace records 2048 successful pad reads and 512 marker calls on one thread;
  pad 0 has two reads at 77 tick values. Thus the current latest-state mailbox is
  not a confirmed per-step input gate. The selected phase includes map/player
  introduction and kickoff, not a moving-play determinism comparison. Clean
  close is exit zero, input desktop unchanged and copied saves remain identical.
  Evidence: `local/netplay-boundary-2026-11-17/summary.json`. No production binary
  or scheduling behavior changed. See [boundary findings](network-engine-boundary.md)
  for the cached-state/edge, initial-state/RNG and scheduler checks still needed.
- Added a host-controlled return from started/interrupted transport sessions to
  the same room's lobby. The request requires its matching session ID, so delayed
  old stop commands cannot cancel a later round. Readiness, frame counters and
  relay budget reset; seats, reconnect credentials and disconnected leases stay.
  A fresh start still requires every reserved participant connected/ready and
  both teams represented. Client session transitions/end/disconnect clear the
  confirmed timeline and queued input while preserving event-queue identity and
  noninput notices. Old-session relays cannot advance the new timeline.
- Transport protocol 2 rejects older peers lacking those lifecycle semantics.
  Real-socket tests complete two rounds in one four-seat room with distinct
  frame-zero inputs, reject an injected delayed old frame 100, and accept the
  new frame one. The interrupted-room case requires the missing player to resume
  and ready before restart. Wrong/old lobby session-ID cases fail before their
  guard and pass afterward. All 16 network cases and 135 aggregate Python tests
  pass (one privilege skip); review found no actionable code issues and the old
  permanent-interruption instruction in server docs was corrected. Evidence:
  `local/release-verification/pre6-build/return-lobby-scoped-*.log`. This is
  transport lifecycle verification, not playable synchronized game recovery.
- Fixed reusable network-client lifecycle failures. A closed client's new
  connect previously retained `disconnected`, so its hello failed with connection
  lost. New streams now start with fresh events/timeline/identity and preserve
  timeline capacity; separately retained credentials can still resume a valid
  lease. Socket-level checks exchange different frame-zero inputs across two
  rooms and verify that old input/events do not leak. Live and pending duplicate
  connects are rejected before replacement; failed handshakes close their reader.
- Review identified and reproduced an overlapping-close race: delayed old stream
  shutdown cancelled mutable `self.task` after a new reader replaced it. Close
  now captures writer/reader references before awaiting. A gated real-stream
  test fails before correction and passes with the new reader alive and a
  successful ping afterward. Shutdown errors also retain reader cleanup.
  All 14 focused network cases and 133 aggregate Python tests pass (one privilege
  skip); follow-up review finds no additional actionable issues. Logs:
  `local/release-verification/pre6-build/client-reconnect-reviewed-*.log`.
  See [transport lifecycle](network-transport.md). Same-room match restart and
  actual simulation synchronization remain pending; no playable-online claim.
- Windowed movie acceptance now includes a natural, unskipped introduction
  through resize / minimize / restore. Read-only samples confirm movie activity
  at all five window actions. Win32 receipts confirm actual minimization,
  restoration to 1280x540, then exact 800x600 and 960x540 clients. Captures show
  progressing movie imagery after restore, the title after natural completion,
  LOAD SUCCESSFUL, Main Menu and responsive Quick Start assignment. Movie opens
  and closes balance, including a two-handle interval; final active count is zero.
  Clean close is exit zero, input desktop stays unchanged and copied save hashes
  remain identical. Evidence:
  `local/acceptance-2026-11-17/movie-window-postpre6/summary.json`. This accepts
  one windowed introduction, not exclusive device loss, all cinematics, physical
  monitor transitions or audio equivalence. CI `44169171890` and `44168400466`
  pass the preceding stencil and reconnect-test commits.
- Added localized stencil GPU coverage beyond both former 4:3 edges. Real 3D
  geometry writes left/right stencil footprints, then the exact inspected world
  resolve shades only those marked pixels. Unmarked center/top, producer bytes
  and stencil/color-write states remain unchanged. The final fixture fails
  against the historical uncorrected bridge (both outer samples white) and
  passes against the current bridge (both `bfbfbf`, controls `ffffff`). All 28
  integration cases pass (`gpu-m_ys8tet`); independent review found no actionable
  issues. Paired receipt:
  `local/world-effect-evidence/localized-stencil-verification.json`. This accepts
  the specific composite with localized geometry, not all real net/shadow scenes.
- Stabilized reconnect acceptance without changing production lease behavior.
  Sensitive tests replace only the server module's time reference; asyncio and
  client timeouts retain real clocks. Successful authenticated reconnect survives
  a real delay longer than the fixture lease while controlled server time stays
  before expiry. Bounded waits observe disconnection and actual cleanup rather
  than assuming fixed scheduling sleeps. A separate socket-level boundary case
  accepts just before the deadline and rejects exactly at / after it; another
  test keeps the real cleaner active and checks removal and host reassignment.
  All seven network cases and 126 aggregate Python tests pass (one privilege
  skip). An in-memory `<=` to `<` mutation fails specifically at the exact-deadline
  assertion; no source mutation is retained. Independent review found no
  actionable issues. Evidence: `pre6-build/lease-tests-python.log` and
  `lease-boundary-mutation.log` under the private release-verification root.
  CI `44167761093` also passes the preceding four-pad diagnostic commit.
- Added explicit private diagnostic ownership for virtual-pad acquisition,
  reusing the PID-local keyboard packet. A stable enabled+diagnostic packet is
  required; missing, normal, disabled, invalid-magic or odd-sequence pages cannot
  bypass focus. Revocation immediately restores lost-input/neutral output. The
  old fixture fails at diagnostic Acquire; the new one covers all four separate
  states and revocation. All 21 named native checks plus absent/late producer and
  actual-DLL factory controls pass. Independent review found no actionable issues.
- The real game now assigns Joypads 1/2 to Arctic Medics and 3/4 to Leo's Lions.
  A direct private page producer exercises each slot individually during active
  gameplay; captures show four colored player indicators and subsequent motion.
  The process closes cleanly, input desktop remains unchanged and copied saves
  match their seed hashes. Evidence:
  `local/acceptance-2026-11-17/four-pad-diagnostics/summary.json`. Physical SDL
  discovery is bypassed by this producer; physical four-controller matches,
  wireless hardware and complete-match acceptance remain open.
- Python rerun passes all 125 tests with one privilege skip after the owned game
  probe closes. The first concurrent run exposes a timing-sensitive reconnect
  fixture: its 200 ms lease expires before the valid resume, returning invalid
  credentials after cleanup. Preserve the failed log and investigate deterministic
  lease-boundary coverage rather than treating that run as green. Logs:
  `local/release-verification/pre6-build/postrelease-pad-python*.log`.
- Four-pad real-game acceptance remains open. A private producer selects the
  SDL-four-device route, but isolated-desktop focus inspection reports no
  foreground window. `VirtualPad::sample` requires foreground ownership for
  foreground-cooperative devices, unlike the explicit private keyboard diagnostic
  path. The stopped probe therefore does not accept four-seat assignment or
  establish a normal foreground input defect. Its corrected producer maintains
  the required heartbeat; the earlier one-shot producer is excluded. Both owned
  probes close cleanly; the fresh probe preserves copied save hashes. Evidence:
  `local/acceptance-2026-11-17/four-pad-fresh-pre6/summary.json`.
  Next: default-deny diagnostic ownership checks, then actual four-seat tests.
- A fresh Western Junction game now passes an observed goal/replay return
  after resize / minimize / restore. Captures show the 1-0 goal (18), low-angle
  replay with sky, net and effects (19), kickoff after Continue (20), and moving
  controlled play after passing/shooting (23). Read-only UI sampling confirms
  active gameplay after the return. Clean process close is exit zero; copied
  save hashes and the input desktop remain unchanged. Evidence:
  `local/acceptance-2026-11-17/replay-window-pre6/summary.json`. This accepts
  one observed transition, not all maps or exclusion of the earlier intermittent
  nearly black replay output; shadow/net edge coverage remains separate.
- Windowed owner-game acceptance on Western Junction now covers two resize /
  minimize / restore cycles, first at kickoff and then during moving gameplay.
  Private desktop window receipts confirm exact 1280x540, 800x600 and 960x540
  clients, actual minimized state and restored dimensions. Read-only UI samples
  confirm active gameplay through the second cycle; captures show resumed
  drawing, moving players, F1 settings and a later replay scene. The process
  closes with exit zero, the input desktop stays unchanged and all copied save
  hashes remain equal to the seed. Evidence:
  `local/acceptance-2026-11-17/gameplay-window-pre6/summary.json` and
  `window-receipts.json`. This is one windowed map; exclusive device loss,
  physical monitor transitions, replay exit and all-map coverage remain open.
- Published [pre.6](https://github.com/MatyMeatBoy/OpenSoccerMania/releases/tag/v0.1.0-pre.6)
  from exact commit `587a7257d32ac84653aeb347611d8b0e86b904fa`; public CI
  `44162146484` passed. Both extracted archive inventories match their file
  hashes; the extracted source runs 125 tests (two expected skips) and the
  extracted portable passes its four-page/import smoke. Downloaded all four
  public assets and matched their bytes to the server SHA-256 digests. Receipts:
  `local/release-verification/pre6-build/published-release.json` and
  `public-download-verification.json`. The release remains a prerelease;
  outstanding acceptance below is not waived.
- Pre.6 rebuild passes 125 Python tests (one privilege skip), 21 named native
  CPU checks and 27 fresh GPU/input/import runs (`gpu-s8otsfoi`). The exact
  rebuilt bridge is used by both those runs and the frozen candidate's smoke.
  Frozen smoke verifies version pre.6, all four pages, synthetic ISO import and
  graphics provisioning. Normal frozen startup with actual install write denial
  and a nonadministrator token verifies fallback storage, small/large window
  icons and clean close; disposable ACLs are restored afterward. Evidence:
  `local/release-verification/pre6-build/`. The rebuilt native image equals the
  Story-accepted image after normalizing only PE timestamp/checksum metadata.
  The unchanged pre.5 candidate is retained at
  `build/released-pre5/OpenSoccerMania/`; its published archives/tag are preserved.
  A fresh owner-game process using the exact rebuilt pre.6 DLL also reloads
  Western Junction and closes cleanly with unchanged saves
  (`pre6-build/game-reload.json`).
- A real Story checkpoint now survives a fresh-process reload. The private game
  starts at City Park, plays through multiple goals/replays and reaches Western
  Junction without manual pitch cycling. Only SAVE0000.SAV changes; the changed
  bank is preserved across clean close. A new process loads all 4800 payload
  bytes successfully and enters Story directly at Western Junction. Reload and
  the second clean close leave all save hashes unchanged. Evidence:
  `story-worker-fixed/summary.json` and `story-progress-reloaded/summary.json`,
  with map captures 3/12 in the first probe and 3 in the second. The final match
  score was not visually captured. This accepts the observed checkpoint round
  trip, not the full campaign, tournament progression or every load failure.
- Exact source commit `9f4a5a2` passes public CI run `44156966977`. An internal
  immutable source ZIP includes the new worker/shader fixtures and branding;
  all 231 inventoried file sizes/hashes verify, and its extracted tree runs
  125 tests with two expected skips. Report:
  `build/tests/source-check-9f4a5a2/verification.json`. This validation artifact
  does not replace the published pre.5 release. All owned game probes are closed
  after the Story reload verification.
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
  evidence root. A fourth process also loaded successfully and exercised Story;
  the checkpoint round trip above supersedes its earlier in-progress status.
  These selected real loads and process closes support the correction;
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
- Completed a real Quick Start match on the ice field: City Footballers 0Ã¢â‚¬â€œ1
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
| Render limits | Added settings for 60Ã¢â‚¬â€œ480 FPS and Unlimited, separate from fullscreen refresh. Actual FPS depends on presentation and workload. VSync-off measurement in progress. |
| Camera/frustum | Camera builder receives the output aspect before constructing projection and CPU frustum. Live 16:9 aspect observed as 1.77778. Edge visibility and multiple aspect ratios still need regression checks. |
| Automated checks | 22 Python tests pass, including unknown-version engine-hook exclusion and adaptive window configuration. Native build succeeds. |
| Borderless hotkeys | F11 and Alt+Enter added, with foreground-only physical-key polling and held-key suppression. Automated F11 message tests passed in both starting modes, including return to previous window placement and clean exit. Desktop stayed 1536Ãƒâ€”864; internal target stayed 1920Ãƒâ€”1080. The backend replaces its window procedure during style changes; the bridge now reinstalls its subclass after that transition. |

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
- A second low-angle trace crossed the far-depth boundary (`0.9998924`Ã¢â‚¬â€œ`1.0001178`). Its black-output regression fails before the widened background predicate and passes afterward. Color-only backgrounds also have a reproduced/passing GPU regression.
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
