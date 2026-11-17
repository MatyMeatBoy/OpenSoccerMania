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
