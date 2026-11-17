# High-FPS animation investigation

## Clock and pose are separate

The reported goalkeeper shake was investigated on the supported x86 executable using an isolated desktop, private input and muted test-process audio. The simulation and animation clocks were sampled at a 50-FPS cap and after switching the same process to a 240-FPS cap. The simulation remained near 50 steps/s. The animation-state time did not accelerate in proportion to rendering.

Diagnostic hooks on animation advance (`0x55a130`) and pose evaluation (`0x55a2a0`) then established one advance/evaluation per actor per simulation step: for example, 72 steps produced 936 advances and 936 pose evaluations for 13 actors. This does not establish every cutscene or procedural effect's clock.

The SA pool at `0x5b75d0` contains `0x368`-byte animation states. It is not a pool of actor objects. Actor objects point into it through actor offset `0x1e4`. An earlier diagnostic incorrectly dereferenced state offset `0x1e4`; the sampler now reads state time at `0x1c`, step at `0x20` and clip at `0x2c`, and rejects unknown executable hashes.

## Boundary rewind

The original snapshot interpolation dispatcher (`0x41fcd0`) uses the current and previous snapshot banks, selected by `0x5e8208`, and the fraction at `0x5e8218`. Its small-fraction threshold is the double `0.05` at `0x570a30`. Inside that threshold it substitutes an interpolation coefficient of **1**.

That behavior causes a discontinuity when multiple rendered frames occur during one simulation step. With snapshot endpoints 10 and 20, fraction 0 displays 20, while fraction 0.25 displays 12.5. The display briefly jumps forward and then rewinds even though simulation time advances correctly. Live sampling found boundary transitions with changing bone quaternions while the animation-state time stayed constant.

The corrected boundary path uses the actual fraction, including zero. Outside the affected boundary interval the original dispatcher remains in use. The replacement advances through the original callback chain using each callback's returned snapshot size, preserving independent current/previous banks. It does not change animation advancement, the authoritative clock, physics or game-speed effects.

## Controls and verification

The experimental Smooth Mode was removed for pre.4 after the reporter observed worse behavior. Old saved preferences are forced off, and the custom boundary dispatcher is no longer installed. The original interpolation path remains active. The first-snapshot history initialization described below stays enabled independently because it fixes uninitialized camera state.

The native regression reproduces the legacy endpoint rewind, checks monotonic progress through zero and the 0.05 boundary, crosses a tick boundary, exercises differently sized snapshot records and verifies that endpoint buffers remain unchanged. Live A/B results are recorded in the work board after validation.

This fixes the identified interpolation mechanism. It does not certify every character, cinematic, map, animation blend or procedural effect. A reporter visual check remains distinct from local counter and regression results.

## First snapshot history and camera zoom

A separate isolated A/B trace identified a startup defect exposed by independent rendering. The allocator at `0x41f7c0` creates both snapshot banks without initializing their contents. The complete save at `0x41fca0` fills only the selected bank. At the first match render, the current camera snapshot contained aspect `1.333333373` and up-Z `1`, while the previous snapshot contained zeros. Interpolating with fraction zero wrote those zeros into the source camera. The next logic save then preserved the corrupted values. The main camera controller refreshes position, target and FOV each tick, but leaves aspect and up unchanged, allowing the error to persist.

This caused nondeterministic zoom changes without changing camera distance: one traced run settled at up-Z `0.4160546` and source aspect about `0.5547394`. Its controller FOV remained `512`, but the horizontal extension used output FOV `1189` instead of the expected `656`. Another startup settled at different values, depending on its initial render fractions.

The fix seeds the previous bank from the **first complete current-bank save after each allocation**, before interpolation can read it. Subsequent saves keep their distinct endpoints. It does not replace the simulation clock or impose a fixed camera lens. The allocator/save hooks use executable signatures, and the regression covers first-frame interpolation, later motion and a new match reusing the same allocation addresses.

The fixed A/B trace records a 6,211-byte seed between the first save and first blend. Both camera endpoints then contain aspect `1.333333373` and up-Z `1`, including at fraction zero. Across 786 live samples over 20 seconds, these values remained constant and output FOV stayed `656`. Vertical projection scale differed from the original by about 0.06%, from integer FOV rounding. Camera position-change diagnostics and within-tick rewinds were zero; the authoritative simulation remained configured at 50 Hz. Private evidence is recorded in `local/camera-goal-evidence/startup-camera-ab.json`.

This verifies the startup history and resulting zoom corruption fix. The reported goal-to-replay-to-gameplay sequence has not yet been reproduced, so its camera behavior remains a separate verification item.
