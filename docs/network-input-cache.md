# Original controller cache and edge state

## Device cache versus gameplay control records

The inspected pad class uses vtable `0x570b4c`. Its update method `0x42eb10`
clears a 272-byte state at object offset `0x2b8`, polls the device and calls
GetDeviceState. The observed return site is `0x42eb72`. Failure leaves a zeroed
cache and follows the original reacquisition path. Valid/acquired state is the
byte at `+0x2b4`; X/Y start at `+0x2b8`/`+0x2bc`, buttons at `+0x2e8`.

The actual all-device pad wrapper is `0x42e440 -> 0x42e680`, dispatching the
pad vtable's `+8` entry. Calls exist in the Windows message pump (`0x4331e4`)
and menu paths (`0x4f252e`, `0x4f2a64`). This refines the earlier boundary report:
`0x410740` is another device-update path, not the demonstrated direct-pad pump.
Its outer-loop position alone cannot establish every raw-pad polling location.
The previous repeated-poll measurements remain valid independently of that
initial attribution.

The game normalizes bindings through `0x42e360 -> 0x42e6b0`. Its live control
records start at `0x638bc4`, with count at `0x638bc0` and stride `0x44`.
`0x485560`, called at `0x406649`, refreshes a record before the pending-step loop.
This is separate from the raw DirectInput cache and its polling frequency.

| Record offset | Inspected meaning |
| --- | --- |
| `+0x00` | Current normalized action mask |
| `+0x04` | Direction angle |
| `+0x08` | Movement magnitude |
| `+0x28` | Released mask: previous AND NOT current |
| `+0x2c` | Pressed mask: current AND NOT previous |
| `+0x30` | Previous mask, updated to current after refresh |
| `+0x38` | Disabled/suppressed control path |
| `+0x3c` | Source device index |

## Snapshot ownership

The engine registers save `0x485620` and restore `0x4856c0` at `0x48023a` /
`0x48023f`. The snapshot has an eight-byte shared header and eleven bytes per
control: angle high nibble/low byte, magnitude byte, then low/high bytes for
current, pressed, released and previous masks. Upper action-mask bits, source
identity and other record fields are not all carried by this compact layout.
It is not a lossless serialization of the whole control object.

This snapshot restoration means raw-poll and edge-update observations cannot be
used alone to assume high-FPS button loss or a safe network injection location.
The relevant native callback ownership must be preserved by an eventual adapter.

## Active-match trace

A separate source-copy diagnostic bridge records after the original refresh
and reads the fields again at the marker. It does not write control fields or
replace the scheduler. Trace collection is enabled only
after the game has reached a normal match view; a private producer alternates
pad 0's A input with nominal 80 ms held / 80 ms neutral intervals.

Over 31.19 seconds, the target record (`0x638bc4`, source device 2) has 1559
refresh samples and 1559 clock-marker samples. Excluding the partial first/last
intervals, all 1558 complete refresh/next-step pairs retain identical current,
pressed, released and previous masks. All 196 observed pressed edges (`0x20`)
reach their next marker. No overwritten-before-marker press is observed. This
selected trace therefore does not reproduce the suspected edge loss.

The array contains six refresh records in this run, including unused sources.
Step logging deliberately reads only its first four records, so it does not
establish complete normalized equivalence for all four assigned pads. Raw
four-pad assignment remains covered separately. No broad timing or performance
claim follows from synchronous diagnostic logging.

The private probe closes with exit zero, leaves the input desktop unchanged and
preserves copied save hashes. Evidence:
`local/netplay-cache-2026-11-17/static-cache.json`, `cache.trace`,
`edge-observations.json`, `summary.json` and source-copy DLLs. Captures are under
`local/acceptance-2026-11-17/network-input-cache/`.

No production hook or input behavior changes in this investigation. The next
adapter work must establish snapshot/update ownership at a complete logical
step, initial-state/RNG equivalence and divergence handling before connecting
network frames to gameplay. These observations do not establish deterministic
replay, authority/state recovery or playable synchronized online matches.
