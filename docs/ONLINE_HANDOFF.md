# Online handoff — 2026-11-18

## Current result

**Two original game instances have not been connected into a synchronized,
playable online match.** Four Python clients can join a room and exchange
frame-tagged inputs locally. That is transport verification, not game integration.
The launcher must not advertise online gameplay as ready.

Workspace: `D:/01Claude/soccermania`, branch `main`. Engineering/evidence baseline
before this handoff: `90409524548e3423bea44a49f7d9935e6e31f9bb`.
The published baseline is
[pre.9](https://github.com/MatyMeatBoy/OpenSoccerMania/releases/tag/v0.1.0-pre.9),
tag/source `e8c340d2ccecc64901009160b04cb9975d4630bf`.
Do not overwrite accepted tags/assets or substitute an experimental probe DLL
for the released bridge.

## What is implemented and verified

- `opensoccermania/netplay/server.py`: standard-library TCP room server;
  four seats, maximum two per team, ready/start barriers, bounded input relay,
  disconnect/interruption, reconnect leases and host-controlled return to lobby.
- `client.py` and `protocol.py`: `NetplayClient`, `Compatibility`, `InputTimeline`,
  protocol **2**, session-tagged inputs and bounded confirmed-frame assembly.
  Matching executable/adapter strings are compatibility checks, not a certificate
  that an engine adapter works. The default adapter identity remains unverified.
- Real-socket tests exercise four clients, different rounds/rooms, stale-session
  input rejection, reconnect cleanup and the race where delayed old close must
  not cancel a new reader. Returning to lobby does not restore game state.
- Four private virtual pads have separately reached the original game's player
  assignment. This establishes neither network-frame consumption by actors nor
  physical four-controller acceptance.

No VPS deployment, external-home match, WAN latency result, deterministic game
replay, cross-process snapshot repair, or playable engine adapter is established.
Do not substitute video streaming or remote keyboard injection and describe it
as the existing simulation-sync design.

## Read these first

1. [Transport lifecycle](network-transport.md),
   [server/API guide](../server/README.md), and
   [socket tests](../tests/test_netplay.py).
2. [Engine boundary](network-engine-boundary.md),
   [input cache/edges](network-input-cache.md), and
   [original recording registries](network-replay-state.md).
3. [Controlled initial state](network-initial-state.md) and
   [random-state ownership](network-random-state.md), including their follow-ups.
4. [Work board](WORKBOARD.md). Other project acceptance work remains separate.

The detailed follow-ups supersede earlier hypotheses inside those investigation
documents. In particular, an empty staging head did not mean recording was absent.

## Engine facts that constrain the adapter

All addresses below belong only to supported owner-executable SHA-256
`067a46c21a1ce7faf1d761041bff62b4188a53f6a484ae1c95f3df4cf1f00ff4`.
Check the executable hash and instruction signatures before using offsets.

| Location | Established ownership / trap |
| --- | --- |
| `0x41fc70` | 37-byte clock/bank marker, **not** a complete game update or proven injection boundary. |
| `0x456260`, `0x524000` | Two mode-dispatch paths. Both can return early; common-update return precedes mode-specific work. Preserve return values. |
| `0x480650` | Common update reloads shared seed from `0x631e78` unless original selector `0x633310 == 1`; increments counters. Not the whole step. |
| `0x480620 -> 0x485560` | Inner normalized-control refresh, after common entry and before original frame recording. It can invalidate assumptions about earlier injection. |
| `0x481380`, `0x4812e0`, `0x481330` | Original recording, save dispatch and restore dispatch. Recording observed; cross-process restoration not verified. |
| `0x406897..0x406942`, then `0x406948` | Additional outer-loop record copies precede interpolation snapshot save. Mode return alone is not this completion point. |
| `0x633308` | Staging registry, consumed into persistent lists. Do not conclude recording is inactive from its later zero value. |
| `0x5e8220` | Separate interpolation chain: 16 nodes / 6211 allocated bytes in the inspected run, containing pointers and unwritten tail bytes. Not a portable authoritative snapshot. |

A bounded trace observes 384 ordered MARKER → MODE_BEGIN → COMMON_BEGIN →
COMMON_END → MODE_END → SAVE sequences on one thread, through the introduction/
kickoff transition. It does not verify moving-play determinism. Initialization
can reset the raw clock within an outer iteration; assign network session/frame
identity explicitly rather than reusing that float blindly.

Normalized controls have stride `0x44`, array `0x638bc4`, count `0x638bc0`:
current `+0`, angle `+4`, magnitude `+8`, released `+0x28`, pressed `+0x2c`,
previous `+0x30`, suppressed `+0x38`, source-device index `+0x3c`.
Do not assume array index equals network seat. The inspected array has six
records, including unused sources, despite four assigned pads.

Compact save/restore `0x485620/0x4856c0` writes 74 meaningful bytes for six
controls but reserves/returns 76. It quantizes/omits fields and has two unwritten
bytes; it is not a lossless control-object or network-packet format. Press edges
surviving to the clock marker do not prove survival through the subsequent inner
refresh, original recording and actor consumption.

## Initial-state comparison: what remains unresolved

Accepted fresh Friendly pairs use LEGO All Stars / Lawbringers, Western
Junction, identical copied profiles/options, and pads 1/2 home, 3/4 away.
At first recording, all 18 bucket-0 components concatenate to 28604 bytes,
with seed/shared RNG 999 and six controls. Reservations total 29176 bytes:
reserved bytes are not all callback output.

Four raw components differ; the other fourteen match in the inspected pair.
All 214 changed aligned words in actor/global/team components have reference
provenance, but there is **no complete portable reference schema**. Equal/null
fields, aliases, referents and state coverage still need classification. The
130 compared owned actor buffers match (22932 bytes per process); this is not
whole-state equality or permission to mask every differing address.

The remaining changed bytes in the 16-byte conditional component map to
`0x6283a0/0x6283c0`. Its flag is observed as 1 around serializer calls in later
runs, so those bytes really are copied. The initializer derives them from the
low byte of an address argument; a reset derives them from saved incoming ECX.
The initializer correlation is observed in run `f`; reset is static-only there.
Their logical use/unusedness is unresolved. Do not normalize them just to pass.

## Latest completed investigation

The shared RNG at `0x5d0a44` and inline RNG at `0x6276e8` are distinct. A shared
per-step seed alone does not establish synchronized initialization or all RNG
ownership. The latest private fixture validates three relocated original inline
routines: 2096 integer, 1572 float and 262 color cases; deliberately corrupting
the integer multiplier makes the check fail. Eight absolute address operands
are relocated; this does not execute the whole game or prove a playable match.

A concrete caller uses a six-color ARGB palette. The inventory contains 51
integer / 97 float / two color direct-call candidates and is not fully classified;
those are static instruction candidates, not runtime frequencies. Do not classify
the entire generator as decorative from one color path.

Two random-derived vector tables also have independent cursors:

- `0x626898..0x6271f8`: 200 × 12-byte records, cursor `0x6271f8`.
- `0x6271fc..0x6276ac`: 100 × 12-byte records, cursor `0x6276ac`.

These are static spans, not captured cross-process equality. Classify gameplay
influence before including/excluding them from authority; synchronizing every
visual cache is not automatically necessary.

## Concrete next work

1. Resume from the latest RNG consumer/table evidence rather than repeating the
   already captured raw comparison. Follow consumers into their outputs and
   determine which affect authoritative rules, which are presentation, and which
   remain unknown. Capture only additional state that decides that distinction.
2. Finish an explicit typed logical-state/reference contract for the selected
   mode/setup. Compare two fresh processes without masking unresolved fields.
   Preserve original behavior; do not globally fix seeds to force apparent equality.
3. Verify the complete input path: exact seat/frame → normalized held/pressed/
   released/previous → inner refresh/recording → actual actor consumption.
   A latest-state mailbox is not a confirmed-frame queue. There is no completed
   confirmed-frame IPC channel between the Python client and x86 game bridge;
   define its bounded queue, session/generation and consumption ownership.
4. Integrate an opt-in adapter at a verified complete-step contract. Waiting for
   missing inputs must keep quit/UI alive and preserve the original logic clock,
   catch-up and snapshot ownership. Do not gate only the clock marker.
5. Run **two actual game instances** with the same setup and confirmed inputs.
   Compare agreed authoritative results over moving play and transitions, inject
   a divergence, and verify the chosen recovery/authority policy. Only then count
   this as game-instance connection; four transport clients are insufficient.
6. Add launcher flow and LAN/WAN acceptance after engine integration works.
   Preserve session isolation during lobby return, reconnect and match restart.

## Evidence and safe execution

All following paths are relative to the current workspace, private/ignored and
not available in a fresh public clone. Do not upload owner inputs, dumps or logs.

| Evidence directory | Primary use |
| --- | --- |
| `local/netplay-boundary-2026-11-17/` | Static flow, exact owner disassembly and anchors. |
| `local/netplay-cache-2026-11-17/` | Normalized controls and bounded edge observations. |
| `local/netplay-completion-2026-11-17/`, `local/netplay-state-2026-11-17/` | Ordered completion traces and interpolation inventory. |
| `local/netplay-registry-2026-11-17/` | Persistent recording registries / actual save callbacks. |
| `local/netplay-initial-2026-11-17/`, `local/netplay-initial-owned-2026-11-17/` | Accepted initial pairs, references and owned buffers. |
| `local/netplay-conditional-2026-11-17/` | Writer provenance, capture `f`, preserved-observer contract. |
| `local/netplay-inline-rng-2026-11-18/` | Latest `verification.json`, `consumer-inventory.json`, `lookup-ranges.json`, private fixture and fresh disassembly. |
| `local/acceptance-2026-11-17/network-*` | Actual menu/assignment/match captures and clean-close/save receipts. |

Use `tools/run_offscreen.py`: actual CreateProcessW STARTUPINFO.lpDesktop,
private input mailboxes and per-PID mute (`local/mute-test-audio.exe`). Never
switch the user's input desktop, synthesize global input or use foreground
launch helpers for tests. Preserve original saves; copy/hash the full initial
profile before launching. Verify menu phases instead of trusting elapsed seconds.
Quick Start chooses different scenes despite identical profiles. The inspected
Friendly profile exposes only Western Junction and City Park; do not assume
the pirate field or locked teams are selectable.

Retain live process handles through observation timeouts; poll the same handle
instead of restarting. Record WM_CLOSE delivery and actual exit; forced stop is
not clean acceptance. Old private scripts may write unused ProbeFPS while
RenderFPS remains 0: inspect the effective INI/native log. Pre.9's public probe
fixes this flag; a requested cap still does not prove measured FPS or logic rate.

No intentionally retained game/test-server process is part of this handoff.
Do not terminate unrelated user processes. Relevant OptiPort network modules
match canonical `a328c5a`; unrelated local controller-guide edits were preserved.

## Recheck commands

From the workspace root, without starting the owner game:

```powershell
.\.venv\Scripts\python.exe -m unittest discover -s tests -p test_netplay.py -v
```

Optional local transport server (not an engine adapter):

```powershell
.\.venv\Scripts\python.exe -m opensoccermania.netplay.server --bind 127.0.0.1 --port 27845
```

Pre.9's recorded full gate is 149 Python tests, 22 named native checks and
31 GPU/input/import cases. Those broad checks do not certify online gameplay.
The focused network command above was rerun for this handoff on 2026-11-18:
all 16 tests passed. Its four-client socket exchange remains transport-only.
