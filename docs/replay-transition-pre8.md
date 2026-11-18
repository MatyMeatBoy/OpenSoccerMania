# Pre.8 replay transition and shutdown follow-up

The released pre.8 bridge was exercised on an isolated Windows desktop with
private keyboard input, per-process audio muting and a copied save profile.
Its SHA-256 was
`aef79419c64e4653fadca8fa3b3e81a6d524ca670c3569951a85ffd973c8f443`.
No production behavior was changed for this investigation.

## Pirate-ship field observation

Adventurers Pilots versus Western Cowboys reached a goal replay with visible
sky, net and effects, returned to a 0–1 kickoff, and continued visible gameplay.
The Continue command was recorded at 301.718 seconds. Later captures retain
gameplay after window resizing; the render log records 1280×540, 800×600 and
960×540 clients. Minimize/restore commands were also requested, but actual
minimized state was not independently captured.

All 42 saved images contain visible output: none has more than 95% of pixels
with all RGB channels at or below 16 (maximum observed fraction 25.78%). This
is a sampled-image check, not continuous frame coverage or proof of correct
shadow/net occlusion. The original intermittent nearly black replay exit did
not reproduce here; its cause remains unproven.

Read-only sampling recorded 40,866 rows through 435.484 seconds. Observed
CIRCLE/STAR fader segments span at most 0.532 seconds. Samples are asynchronous
reads, not atomic engine snapshots. The legacy trace column named `paused`
reads address `0x661310`: static inspection shows this is the fader's final-frame
presentation latch, **not a gameplay pause flag**. The fader update/draw path
at `0x4e2940` presents its completed state before clearing that latch and the
active fader. Do not infer game pause from that column.

The run required forced termination after its cleanup posted WM_CLOSE and
WM_QUIT together. It returned exit code 1 and is **not a clean full-run pass**.
The three copied save files remain byte-identical and the input desktop was
unchanged. The visual observation and failed shutdown are separate results.

Private evidence: `local/acceptance-2026-11-17/replay-transition-pre8/`, including
`fader-summary.json`, `image-metrics-final.json`, command receipts and captures.
Private owner content is excluded from public source and release archives.

## Shutdown control

A separate early-menu control records owned windows and the result of message
delivery. WM_CLOSE was successfully posted to the actual SD Window. The
process exited with code 0 in 0.203 seconds without the WM_QUIT fallback; the
input desktop and copied saves were unchanged. This establishes a successful
menu close, not the root cause of the longer run's failure or match shutdown.

Private evidence: `local/acceptance-2026-11-17/exit-control-pre8/`, especially
`close-receipt.json` and `result.json`.

A second control reaches active Shipshape Shipmates versus City Footballers
gameplay on the pirate-ship field (captures at 160 and 175 seconds). At the
185-second deadline, WM_CLOSE delivery to the recorded SD Window succeeds;
the process exits 0 in 0.203 seconds, without WM_QUIT or forced termination.
The private input desktop remains unchanged. This entry uses its newly created
private profile and generated configuration rather than the longer run's exact
copied starting profile; it is a bounded match-close observation, not a matched
A/B proof that changing cleanup messages fixes the previous failure. It does
not cover closing after a goal replay or the resize sequence. No save-preservation
comparison is claimed for this second control because initial hashes were not
recorded. Its legacy shutdown output includes a negative `MEM LOST IN FE`
diagnostic and reference-node messages; exit zero does not establish leak freedom.

Private evidence: `local/acceptance-2026-11-17/exit-match-control-pre8/`.
