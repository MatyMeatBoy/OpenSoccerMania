# Original logical serialization registry

## Staging list and persistent lists

The empty head at `0x633308` observed in the prior probe is a consumed staging
list, not evidence that logical serialization is inactive. Fresh disassembly of
the supported owner executable shows initializer `0x480ec0` walking that list,
passing each node to `0x481250`, allocating buffers through `0x481080`, then
clearing the staging head at `0x480f9e`.

`0x481250` partitions the nodes by their numeric registration flag into three
persistent lists. Heads are `0x63324c + 4*bucket`, counts are
`0x633240 + 4*bucket`, and next links are node offset `+0x10`. The persistent
node layout is object at `+0`, save at `+4`, restore at `+8`, extra callback at
`+0xc`, next at `+0x10`, reserved byte size at `+0x14`. Its meanings must not be
confused with the separate interpolation-chain node layout.

A real four-pad Quick Start probe captures 24 registrations and two read-only
persistent inventories. Every registration matches an actual persistent node
by object, save, restore, extra and reserved size; list counts and stable heads
also verify:

| Bucket | Nodes | Sum of registered byte reservations |
| --- | ---: | ---: |
| 0 | 18 | 29176 |
| 1 | 1 | 76 |
| 2 | 5 | 2380 |

The sole bucket-1 node is the compact control serializer: object `0x638bb8`,
save `0x485620`, restore `0x4856c0`. Bucket 0 includes the global block at
`0x631e60` with save/restore `0x480aa0` / `0x480ad0`; its inspected 216-byte
copy includes the per-update seed counter at `0x631e78`. This is a component
inventory, not proof of complete game-state coverage or portable pointer layout.

## Actual save calls

Diagnostic source-copy hooks preserve original arguments, results and Win32
last-error state. Bounded initial and post-kickoff windows record 96 save-dispatch
returns and 96 control-save returns on one thread. Dispatch samples comprise
two bucket-0 and 94 bucket-1 calls. Their counts differ from control-save samples
because each hook has its own bounded recording window. No restore-dispatch or
control-restore event is observed in this selected run; no replay was requested.
The registered restore capability is therefore distinguished from exercised
restoration.

All 96 control-save calls see six controls and return an end pointer 76 bytes
past the supplied destination. The meaningful writes are an eight-byte shared
header plus eleven bytes per control: 74 bytes here, followed by two bytes not
written by the inspected callback. Size routine `0x485600` computes
`12 + 4 * floor(11*n/4)` for the observed nonnegative counts, rather than simply
returning the meaningful byte count. Preserve the original reserved size and
return stride; do not hash its padding as logical input.

The two bucket-0 dispatch samples advance by 28604 bytes, whereas the sum of
registered reservations is 29176. Reservations and observed callback output
lengths are distinct; a future exporter must validate individual callback
contracts and ring wrapping rather than serialize a reserved span blindly.
All 96 observed control saves have `seed_counter == frame_counter + 999`.
This checks their association with original frame recording in this run, not a
network-session frame identity across initialization or recovery.

## Input refresh precedes original recording

Fresh control flow identifies another refresh site beyond the earlier outer
loop call at `0x406649`. Common update `0x480650` calls `0x480620` at
`0x480673`; that routine loops over every control and calls `0x485560` at
`0x480633`. It runs after the common entry reseed and before the conditional
recording call to `0x481380` at `0x480699`. The latter uses save dispatcher
`0x4812e0`, whose callbacks return advancing byte pointers. Restore dispatcher
`0x481330` uses the corresponding restore entries.

Thus the inspected common-step path refreshes controls before it records them,
and it still returns to later mode-specific simulation work. Earlier observations
of press edges surviving from a refresh to the clock marker do not establish
that those edges survive this subsequent refresh or reach actor consumption.
The selected cache trace remains valid for its measured interval; a future
adapter must additionally trace caller identity and the actual recorded controls.
Do not assume a wait/injection at the clock marker or common entry alone preserves
edges. Any input override must account for the inner refresh and original
recording/restore contracts, as well as agreed initial state and scheduler waits.

## Verification and remaining work

The private process closes with exit zero, the offscreen runner confirms the
input desktop remains unchanged, and all three copied save hashes match.
Evidence is under `local/netplay-registry-2026-11-17/`: `summary.json`,
`definitions.trace`, `registry.trace`, `initial-inventory.json`,
`late-inventory.json`, fresh lifecycle disassembly and source-copy DLL. Captures
are under `local/acceptance-2026-11-17/network-registry/`; the post-kickoff window
is enabled at approximately 195.2 elapsed seconds after phase confirmation.
Synchronous file tracing prevents performance acceptance from this run.

Next work is a controlled identical map/team/options setup in fresh processes,
with component-level initial-state comparison, pointer/padding inventory and
ordered input-refresh/recording observations. Original replay serialization is
a useful ownership lead, not yet a cross-process snapshot format, complete
checksum, recovery implementation or playable network adapter. Production input,
scheduler, seeds and published pre.7 binaries are unchanged.


A subsequent [controlled initial-state comparison](network-initial-state.md)
captures all 18 bucket-0 components in two fresh Friendly setups. Four raw
components differ; pointer/conditional-write ownership and logical equality
remain unproven. This comparison advances the initial-state investigation without
accepting raw aggregate equality as a complete synchronization contract.
