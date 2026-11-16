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

The launcher exposes **Smooth animation interpolation at high FPS**. `Graphics.SmoothInterpolation` defaults to 1 in the native configuration and can be disabled for comparison. Installation requires the existing executable identity gate and a matching dispatcher signature. Diagnostic traces report corrected boundary frames and within-tick coefficient rewinds. `Diagnostics.TraceAnimation=1` enables bounded animation-state counters; normal configurations do not install those diagnostic animation hooks.

The native regression reproduces the legacy endpoint rewind, checks monotonic progress through zero and the 0.05 boundary, crosses a tick boundary, exercises differently sized snapshot records and verifies that endpoint buffers remain unchanged. Live A/B results are recorded in the work board after validation.

This fixes the identified interpolation mechanism. It does not certify every character, cinematic, map, animation blend or procedural effect. A reporter visual check remains distinct from local counter and regression results.
