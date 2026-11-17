# Original engine input/step boundary

## Verified control flow

The owner executable's existing `simulation_step` hook at `0x41fc70` is a
37-byte clock/bank marker. Its body increments the float at `0x5e8210`, alternates
the snapshot-bank selector at `0x5e8208`, and returns. It contains no device poll
or complete game-update body. The steady-state marker count can identify logic
iterations; it is not a verified place to inject a complete network frame.

| Location | Inspected operation |
| --- | --- |
| `0x406630` | Calls a generic device-update path `0x410740(0)`; not the direct-pad pump identified below. |
| `0x406704` | Queries the pending iteration count via `0x41fc10`. |
| `0x40671c` | Calls the clock/bank marker; its return site is `0x406721`. |
| `0x406885` / `0x40688d` | Surrounding mode-dependent dispatch calls; full update ownership remains unverified. |
| `0x406948` | Saves snapshot records through `0x41fca0`. |
| `0x406961` | Iteration backedge to `0x406718`, bypassing the outer device update. |
| `0x42eb6f` | Calls DirectInput GetDeviceState for 272 bytes; return site `0x42eb72`. |

The inspected backedge bypasses the named generic outer device-update call.
It does not by itself establish all raw-pad polling locations. Follow-up cache
inspection identifies the actual pad wrapper and the normalized gameplay control
refresh separately; see [cache/edge ownership](network-input-cache.md). The
runtime observations below establish repeated pad reads at the same recorded
tick without depending on the initial generic-update attribution.

## Isolated runtime ordering

Source-copy diagnostic DLLs append bounded observations without rewriting game
input, time or native production sources. The real private-desktop game assigns
four virtual pads, enters its map introduction/player cameras and reaches
kickoff. All 2048 pad samples (512 per slot) return success from `0x42eb72`;
512 clock-marker observations return to `0x406721`. Every observation occurs on
one thread, with monotonically ordered performance-counter timestamps.

Pad 0 is queried at 435 distinct recorded tick values; 77 values have two
queries. Each slot has the same 512-query total. Thus device polling is not a
one-query/one-network-frame boundary. The trace includes an initial setup clock reset;
these measurements are not a continuous full-match or moving-play determinism
comparison. Logging affects execution cost, so no FPS or latency claim follows.

The probe closes with exit zero, leaves the input desktop unchanged and preserves
all copied save hashes. Private evidence:
`local/netplay-boundary-2026-11-17/static-boundary.json`, `summary.json`,
`net-boundary.trace` and source-copy DLLs. Captures are under
`local/acceptance-2026-11-17/network-step-boundary/`.

## Required next integration work

A future adapter must identify the cached normalized controller states and edge
handling consumed by each logical update, then deliver exactly the confirmed
four-seat frame there. Repeating the current latest-state pad mailbox or waiting
inside the clock marker alone does not establish this property: the normalized refresh and snapshot restoration have their own ownership, and
the inspected iteration backedge bypasses that outer refresh.

It must also verify initial map/team/state/RNG equivalence, authoritative game
state checksums, scheduler behavior while waiting, and divergent-state recovery
or a host-authoritative design. Menus, cameras and kickoff input observations do
not establish those properties. Native four-seat assignment and the network room
relay remain separately verified components; they are not yet a playable online
match adapter.

## Follow-up on mode dispatch

Both inspected mode paths call common routine `0x480650`, which reloads a
per-update random seed and increments its counters before returning. Their
mode-specific work continues afterward. The
[random-state investigation](network-random-state.md) verifies that behavior in
two live windows and limits its usefulness as a future adapter/checksum boundary.

## Mode completion and skipped updates

Fresh disassembly of the hash-verified executable refines the boundary further:

- At `0x406868`, a nonzero `ebp` skips both mode dispatch calls and goes to
  `0x406897`. The clock marker has already run in this iteration. Therefore a
  clock-marker observation does not, by itself, prove a common or mode update.
- With dispatch enabled, the value returned by `0x510180(edi->field_18)` selects
  the path: a value other than 2 uses `0x456260`; value 2 uses `0x524000`, except
  that byte `0x5bf540 == 4` also selects `0x456260`. These are observed numeric
  selectors, not verified names for game modes.
- Both functions have an early-return path before `0x480650`: when
  `0x633288 != 0` and `0x633238 != 1`, they call `0x481ca0` and return zero.
  Thus even entry to a mode dispatcher does not establish an advanced update.
- On their common-update paths, both perform more work afterward, including
  actor-related loops, `0x512850` and `0x45cab0`. Their final return instructions
  are `0x456406` and `0x5240d1`, respectively. Return values flow into `ebp` in
  the outer loop and affect continuation; a diagnostic wrapper must preserve
  the original return value as well as calling convention.
- After either mode call (or its skip), `0x406897..0x406942` iterates the count
  at `0x636200`. Each record has stride `0x78` at `0x5cc30c`. It first copies
  three existing words into globals `0x5ccc68..0x5ccc70`, then replaces the
  record's first three words from `0x4822f0(index)` and four further words at
  offsets `0x24..0x30` from object offsets `0x0c..0x1c`. No field names or
  completeness of gameplay state are established by those copies.
- Only after that loop does `0x406948` call the snapshot saver. The saver walks
  nodes from `0x5e8220`, invokes each `node+4` callback with object and current
  bank destination, and advances the destination by four times the callback's
  return value. This is the existing interpolation serialization contract;
  it is not evidence that every authoritative gameplay field is serialized.

The next bounded runtime probe should pair marker, mode entry/return, common
entry and completed snapshot observations on the same thread, recording selector,
early-return flags and seed counters. It must distinguish an iteration with a
skipped update from an actual update before assigning network frame numbers.
An initial-state comparison must also inventory serialization callbacks and
exclude process pointers/padding rather than hash a whole process or blindly
accept interpolation-bank equality as complete game-state equality.

All 24 selected instruction anchors match both the existing full disassembly
and freshly disassembled bytes of owner executable SHA-256
`067a46c21a1ce7faf1d761041bff62b4188a53f6a484ae1c95f3df4cf1f00ff4`.
Private receipts are `local/netplay-boundary-2026-11-17/mode-completion-static.json`
and `mode-completion-fresh.asm`. This is static control-flow evidence; runtime
frequency, cross-process state equality and a playable adapter remain unverified.
