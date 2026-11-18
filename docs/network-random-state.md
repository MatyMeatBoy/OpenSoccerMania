# Random-state ownership before network synchronization

## Inspected shared generator

The supported owner executable stores one shared 32-bit random state at
`0x5d0a44`. Setter/getter routines are `0x417240` / `0x417250`. Integer
`0x417260` and float `0x4172a0` advance it with unsigned wraparound:

`state = state * 0x41c64e6d + 0x40584c0f (mod 2^32)`

The bounded integer result is `(state % 131071) % bound`; a zero bound returns
zero but still advances the state. This describes inspected machine operations,
not a replacement generator installed by OpenSoccerMania.

Startup calls the setter at `0x449aed` with `timeGetTime` output. Other code
uses temporary save/seed/restore scopes, for example `0x41c5b0`, `0x41c78c`
and `0x4c707a`. A separate inline-generator state at `0x6276e8` is initialized
through another timing path at `0x452044`. Therefore one shared seed is not an
inventory or synchronization solution for all random state.

## Common-update seed counter

Initialization at `0x47fdde` sets `0x631e78` to 999 and clears the related
counter `0x631eec`. The common routine `0x480650` loads that seed into the shared
generator at its entry when mode `0x633310` is not 1, then increments both
counters before returning. Calls occur inside both mode-dispatch paths at
`0x456295` and `0x524027`.

This function is not the complete simulation update. The inspected callers
continue mode-specific work after it returns. Its entry is a candidate for
studying confirmed input delivery, but its exit is not a verified whole-step
checksum or complete state-snapshot boundary.

## Bounded runtime verification

A private source-copy DLL records BEGIN, the existing seed setter invocation
(return site `0x480670`) and END. It does not override the supplied seed or
replace the original update. Diagnostic file operations preserve Win32 last-error
state. All records occur on one game thread with ordered counter timestamps.

The first 128 triples begin at seed 999 and finish with seed 1126 loaded / 1127
stored. A separate active-match window contains 128 more triples, loading seeds
5547 through 5674. In all 256 cases the setter receives the current counter,
the shared state immediately equals that seed, and both counters increment by
one on return. Observed mode is zero throughout.

In these samples the shared state at END still equals the loaded seed. Across
all 254 adjacent pairs within the two windows, the next BEGIN instead sees a
different shared state. This is evidence of generator use between common-update
calls; it does not identify every consumer or attribute all such use to rendering.
In particular, mode-specific work continues outside the sampled function.

The probe closes with exit zero, copied save hashes remain identical and the
input desktop stays unchanged. Private evidence is
`local/netplay-rng-2026-11-17/static-rng.json`, `rng.trace`, `summary.json` and
source-copy DLLs. Captures, including the normal match view before enabling the
late window, are under `local/acceptance-2026-11-17/network-rng/`.

## Remaining requirements

No production hook or seed behavior changes in this pass. Per-update reseeding
does not establish identical initial actors, map/team/options/assets, all random
state, floating-point behavior or complete step results across processes.
The adapter must compare agreed logical-state checksums in controlled identical
setups, verify input/snapshot ownership and scheduler behavior while waiting,
and implement divergence handling or host authority. Playable synchronized
online matches remain unimplemented.

## Follow-up: inline generator and particle-color path

Fresh inspection and a private instruction-level fixture refine the second
generator at `0x6276e8`. Its integer routine `0x452f30` uses the same unsigned
LCG constants as the shared generator, but reads/writes this distinct state.
Bound zero still advances it. Float routine `0x452f70` multiplies the remainder
by its float argument and the original stored scale; it does not call the
shared getter/setter. This is static state ownership plus bounded arithmetic
verification, not proof that the two subsystems can be synchronized independently.

Routine `0x452dd0` advances the inline state and selects one of six ARGB values
at `0x5acd7c`: red, green, blue, yellow, magenta and cyan. Its caller at
`0x45eb61` consumes the result with alpha/RGB masks. The inspected surrounding
initialization also names a ParticleSystem/Particle Flush path. This resolves
a concrete color-selection consumer; it does **not** classify every consumer
of the inline generator as decorative or safe to exclude from logical state.

There are 51 encoded direct-call candidates for the integer routine, 97 for the
float routine and two for the color selector in the inspected linear `.text`
disassembly. Each selected E8 encoding/relative destination matches the exact
supported executable bytes. These are static candidates, not runtime counts,
complete indirect-call coverage or an authoritative-state classification.

### Bounded original-instruction checks

The private fixture verifies the entire input executable's supported SHA-256,
then copies only the three pure routines above into its own executable pages.
Eight documented absolute state/constant address operands are relocated to
fixture storage; all other instruction bytes and local branches are preserved.
It calls no game entrypoint, constructors, imports or full update. Original
content and copied code remain private.

Across 262 explicit/generated seeds, the fixture checks 2096 integer cases,
1572 float cases and 262 color cases. This includes unsigned wrap, zero/large
bounds and zero/positive/negative float factors. All pass. A one-ULP float
tolerance was declared before execution; maximum observed difference is zero
in the recorded x87 environment. No cross-platform/precision guarantee follows.
A deliberately corrupted integer multiplier makes the same fixture fail as
expected. Both isolated processes preserve the input desktop.

An earlier whole-image mapping attempt was excluded after the fixture's desired
address range was already occupied. That observation is a fixture limitation,
not a game crash or evidence about the replay failure. The final fixture uses
relocated function copies and records its source/binary hashes separately.

### Random-derived tables and cursors

The inspected initialization calls `0x452ae0` before the time-derived seed store
at `0x452044`. Its loops build two vector tables. Fresh inspection of the called
transform confirms three float outputs; the static store ranges and consumer
strides/wrap points agree:

| Table span, end exclusive | Layout | Consumer cursor |
| --- | --- | --- |
| `0x626898..0x6271f8` | 200 records × 12 bytes | `0x6271f8`, wraps at 200 |
| `0x6271fc..0x6276ac` | 100 records × 12 bytes | `0x6276ac`, wraps at 100 |

Consumers `0x452c60/0x452cb0` and `0x452d20/0x452d70` copy entries and advance
the corresponding cursors. Thus a seed value alone does not describe these
random-derived tables and their current positions. Initial contents, other
writers and gameplay influence remain unverified. They must be classified
before deciding whether they belong to authoritative state; visual caches must
not automatically be added to a network contract merely because they differ.

No production seed, input, snapshot or scheduler behavior changes. No bytes are
masked, no whole-state equivalence is accepted, and online gameplay remains
unimplemented. The next investigation can target the remaining consumers and
table/cursor ownership rather than treating every inline-RNG difference as
proven gameplay divergence. Private evidence is under
`local/netplay-inline-rng-2026-11-18/`: `verification.json`,
`consumer-inventory.json`, `lookup-ranges.json` and fresh instruction inspection.
