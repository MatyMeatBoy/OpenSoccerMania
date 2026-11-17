# Controllers

Controllers stay separate from the physical keyboard. SDL reads up to four
physical pads and publishes four independent states. The game's private
DirectInput proxy exposes one device with its own GUID for each connected slot;
it suppresses duplicate physical-pad enumeration while that route is active.
No system-wide virtual driver is installed and no desktop keys are synthesized.

Both the left stick and D-pad feed the same movement axes for that pad. The
D-pad is consumed before native input is published; it cannot also move a
keyboard player or become a second POV direction. Existing connected pads keep
their slots when another pad disconnects. A fifth controller is reported as
outside the four-pad limit rather than combined with another controller.

The original game's controller-assignment screen assigns these independent
sources to player seats. Its input manager can enumerate sixteen pads in addition
to keyboard/mouse, and the Windows manual supports up to four players in Quick
Start/Friendly mode. A keyboard remains an independent source; four simultaneous
player seats are not five seats when a keyboard is also connected.

## Controls

Gameplay follows the PS2 button positions, shown with Xbox letters:

| Xbox input | PS2 position | Action |
| --- | --- | --- |
| A | Cross | Pass / tackle / run toward ball |
| B | Circle | Shoot |
| X | Square | Long ball |
| Y | Triangle | Power-up |
| LB | L1 | Sprint |
| RB | R1 | Switch player / skill move |
| Left stick or D-pad | Left stick or D-pad | Move / aftertouch |
| Start / Menu | Start | Pause, on release |
| View / Select | Select | Skip a cinematic, on release |
| View + Start | Select + Start | Open/close internal settings |

PC menus use A to confirm and B to go back. Native menu/match layouts switch
from the published game state; the original pause-menu flags select menu controls
without waiting for the simulation heartbeat to expire. The launcher's manual
mode remains a fallback for an unrecognized native state. Internal settings
always consume pad input and use A/B, stick/D-pad and LB/RB section navigation.

Start and View are both deferred until release. Pressing their chord in either
order consumes both standalone actions and emits F1 once, rearming after both
buttons are released. Standalone View emits only a movie-gated private Escape
command; it is not also sent as a native pad button. Start is only a native pad
pause button. Held inputs are blocked across settings/menu context changes until
released, preventing a settings action from becoming an in-game action.

The mappings are checked against the original
[Windows manual](https://oldgamesdownload.com/manual/football-mania-windows-manual-english/)
and [PS2 manual](https://www.videogamemanual.com/PS2/LEGO%20Soccer%20Mania%20%28USA%29.pdf).
The virtual device reports the exact legacy Gravis instance name so the original
game selects its verified button profile. Its product label and four private
GUIDs identify the OpenSoccerMania devices; it does not impersonate hardware
outside this game's process.

## Button styles

Auto uses SDL's mapped controller type, with a name fallback for raw devices.
Choose Xbox 360, PlayStation 4, Switch Pro or Steam Deck in launcher Controls /
Graphics or the F1 panel to override the visual family. This changes hints only;
input positions and separate player slots stay the same. The existing button
hints on/off preference remains available.

| Position | Xbox 360 | PlayStation 4 | Switch Pro | Steam Deck |
| --- | --- | --- | --- | --- |
| South: confirm / pass | A | Cross | B | A |
| East: back / shoot | B | Circle | A | B |
| West: long ball | X | Square | Y | X |
| North: power-up | Y | Triangle | X | Y |
| Pause | Start | Options | Plus | Menu |
| Video skip | Back | Share | Minus | View |

The pause and skip buttons together open F1 settings. Steam Input can expose a
controller as XInput, concealing its original family; use the manual style in
that case. Steam-family glyph detection is a display convention, not proof that
the device is a Deck. Rear buttons, trackpads, Linux/Proton Gaming Mode and
physical PS4/Switch/Deck devices still need their own acceptance.

Glyphs come from [Xelu's CC0 prompt pack](https://thoseawesomeguys.com/prompts/).
The selected originals, source hashes, license and deterministic native atlas
are included in the public package.

## Runtime and integration

`ControllerBridge(root, pid=None, log=None)` retains `start()`, `stop()`,
`enabled`, `selected_id`, `devices`, `status`, `deadzone`, `mode` and
`automatic_mode`. Device entries include `id`, `name`, `mapped`, and `slot`
(1â€“4, or `None` beyond the limit). `selected_id` chooses settings navigation,
not a source to aggregate into gameplay. Every assigned pad is published.

Recognized SDL gamepads use standardized positions. `raw_mapping` translates
unrecognized joystick button indices into SDL positions; the default assumes a
common Xbox raw layout. Raw hats and the first two axes also work. Nonstandard
adapters require a matching raw profile; raw button numbering is not universal.
The worker constrains the stick deadzone to 0.05â€“0.95.

The native proxy waits at most 300 ms for the SDL producer at initial discovery.
If it is unavailable, the proxy preserves the original physical DirectInput
route. Once chosen, it keeps one route for the process so late discovery cannot
create duplicate pads. The virtual route detects a stopped producer within
500 ms, neutralizes state, and reports disconnected input. An in-progress page
write uses the last fresh stable state rather than spuriously disconnecting a pad.

## Private protocols

Keyboard/settings: `Local\OpenSoccerMania-Input-<pid>`, 272 bytes. Four DWORDs:
magic `0x494d534f`, sequence, flags, controller-activity serial, followed by 256
DirectInput key bytes. Flags are 1 enabled, 2 explicit private diagnostics,
4 controller connected, 8 recent controller activity, 16 explicit movie skip.
The activity serial changes on actual controller input transitions, never device
presence. Its default remains zero for existing diagnostic callers.

`InputMailbox.write(keys, enabled=True, background_test=False,
controller_present=False, controller_active=False, activity_serial=0,
movie_skip=False)` preserves the packet size. Outside internal settings, native
keyboard merging accepts full keys only with explicit diagnostic flag 2. Movie
flag 16 permits only Escape and only while the native movie flag is active.
Gameplay movement/actions never travel through this keyboard page.
Virtual-pad acquisition also honors flags 1+2 only from a stable PID-local packet
for isolated diagnostics. Missing, disabled, odd-sequence or invalid-magic packets
retain normal foreground rejection; revocation returns neutral input immediately.
The authorization packet has no heartbeat timeout of its own; the pad state page
still expires after 500 ms without updates. Production SDL input never sets flag 2.


Pads: `Local\OpenSoccerMania-Pads-<pid>`, 160 bytes. Eight header DWORDs: magic
`0x504d534f`, version 1, sequence, flags, four reserved. Flag 1 enables input;
flag 2 selects match button layout. Four 32-byte slots follow: SDL instance ID,
connected flag, signed X/Y in -32767â€¦32767, standardized SDL button mask, and
three reserved DWORDs. Mask bits 15/16 represent left/right triggers. D-pad bits
are absent because movement has already been resolved into X/Y. An absent slot
is all zero. The native reader honors each device's requested data format and
axis ranges, returns neutral POVs and unused velocity/acceleration/force axes,
and preserves slot identity through native device creation.

UI: `Local\OpenSoccerMania-UI-<pid>`, 32 bytes, eight little-endian DWORDs:
magic `0x554d534f`, version 1, flags, revision, four reserved. Flags are 1 settings
open, 2 gameplay active, 4 movie active. The controller only opens an existing
mapping and never creates a read-only page before the native publisher.

## Verification limits

Python regressions cover independent stable slots, one movement channel,
keyboard isolation, the page layouts, serials, wraparound, both chord orders,
held/released gestures and settings state. Native regressions cover four distinct
GUIDs/states, the game's standard 272-byte/164-object format, PS2/legacy button
translation, menu changes, axis ranges, buffered input, focus, panel handoff,
disconnects, stale/unstable pages, and movie-gated Escape. An additional headless
run loads the actual proxy DLL and exercises four devices through the real system
DirectInput factory. Missing and late producers have separate regressions.

These checks do not constitute a four-physical-controller match test. Physical
hardware, wireless reconnects, vendor-specific raw layouts and complete matches still require verification with the owner's controllers.
A private direct-page producer has assigned Joypads 1/2 and 3/4 to opposing teams
in the real game and exercised individual inputs with four player indicators.
This accepts the native four-seat route, not physical SDL discovery or hardware. No
online transport is introduced by this local four-slot input protocol.
