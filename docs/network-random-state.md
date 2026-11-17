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
