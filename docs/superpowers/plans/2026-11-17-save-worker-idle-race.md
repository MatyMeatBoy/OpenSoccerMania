# Save worker idle-race investigation

## Evidence and scope

The isolated `story-progress-assisted` probe captures LOAD FAILED at 63 seconds.
Its external read-only trace records load phase 1, phase 2, then phase 14 with
operation result 2, status 0 and no expected/read payload bytes. The profile is
still byte-identical to the Story-team seed. The later mistaken empty-slot
navigation is separate and never confirms a new save name.

In the supported executable, worker dispatch at 0x43047d uses a command snapshot
in EBX. An idle snapshot takes the out-of-range branch to 0x4306fa, which clears
the shared pending command unconditionally. A producer can enqueue between the
snapshot/unlock and that clear. The worker then erases the new request without
executing it. Load phase 2 can consequently receive a null device result with
success status, before opening a save file. Other overlapping-consumer/busy-queue
failures are not yet excluded.

## Implementation and verification

Completed so far: deterministic old-fail/new-pass production-DLL fixture,
signature-scoped guard, all 27 integration cases, 125 Python tests and independent
review of the emitted assembly. Three fresh patched owner-game loads read their
full payload, leave save hashes unchanged and close with exit zero. The fixture's
stop control reaches the yield continuation, not the full original thread exit.
Story progression and causality of every observed load failure remain unproven.

1. Reproduce the lost-request window deterministically with synthetic executable
   padding and the production DLL. Model a zero snapshot and a newly queued
   command; preserve idle, valid-command and invalid-command controls.
2. Signature-check the surrounding snapshot/unlock sequence, dispatch and idle
   continuation. Add a narrowly scoped x86 dispatch guard that skips completion
   clearing only for an observed zero command. Keep normal/invalid dispatch and
   the original worker implementation unchanged. Do not turn failures into
   synthetic success or modify save files.
3. Check unsupported-code exclusion, register/stack preservation, a retained
   request processed on the next pass and existing metadata/status behavior.
   Run native build, fresh integration regressions and Python checks.
4. Obtain independent review of the mid-function hook and evidence before
   claiming correction. Repeat real profile loads with the diagnostic status
   logger disabled and verify bytes/clean close. Preserve all failed attempts.
5. Continue actual Story completion/save/reload acceptance separately; neither
   the synthetic race nor a few successful loads proves universal progression.
