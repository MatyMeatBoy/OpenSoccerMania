# Controllers

The launcher uses the bundled SDL3 runtime, including its XInput, DirectInput and
HIDAPI backends. Recognized gamepads use SDL's standardized Xbox positions.
Unrecognized devices use the raw joystick fallback. Discovery refreshes every
half second, releases disconnected devices, and chooses the selected device or
the first available device. Controller input is written only to the game's
private DirectInput mailbox while its process owns the foreground window.
No desktop keyboard input is synthesized.

| Xbox input | Menus/settings | Match |
| --- | --- | --- |
| A | Confirm | S |
| B | Back | D |
| X / Y | — | A / W |
| LB / RB | Settings section previous/next | Q / Shift |
| D-pad or left stick | Navigate | Move |
| Start | Pause on release | Pause on release |
| View/Select + Start | Open/close settings | Open/close settings |

Start is reserved until release so either order of the settings chord suppresses
pause completely. The chord emits F1 once and rearms after both buttons are
released. Gesture pulses last 80 ms so normal game polling can observe them.
Start does not emit pause while settings are open. F1 on a physical keyboard also
opens settings through the native runtime.

## Launcher integration

`ControllerBridge(root, pid=None, log=None)` retains `start()`, `stop()`,
`enabled`, `selected_id`, `devices`, `status`, `deadzone`, `mapping` and `mode`.
`devices` entries contain `id`, `name`, and `mapped`. `automatic_mode` defaults
to true. Native gameplay state selects Match automatically; native menu state
selects Menu. Setting `automatic_mode=False` honors the manual `mode` outside
the settings panel. Without a valid native UI page, `mode` is the fallback.
The settings panel always uses Menu controls, including when manually overridden.

`mapping` maps Menu/Match names to SDL button-index/key-name dictionaries.
View (4) and Start (6) are reserved for settings/pause. `raw_mapping` translates
raw device button indices to SDL gamepad indices; its default assumes the common
Xbox raw order (A/B/X/Y, LB/RB, View/Start, stick clicks). Raw hats supply D-pad
navigation and the first two axes supply movement. Nonstandard raw layouts need
a device-specific `raw_mapping`; no universal raw button numbering exists.
Stick deadzone is configurable and constrained to 0.05–0.95 by the worker.

## Private protocols

Input: `Local\OpenSoccerMania-Input-<pid>`, 272 bytes. DWORD magic
`0x494d534f`, sequence, flags, reserved, followed by 256 DirectInput key bytes.
Flags: 1 enabled, 2 explicit private test, 4 connected selected controller.
`InputMailbox.write(keys, enabled=True, background_test=False,
controller_present=False)` preserves the old callers and packet size. Sequence
is odd during publication and even when stable. F1 uses DIK 0x3b.

UI state: `Local\OpenSoccerMania-UI-<pid>`, 32 bytes, eight little-endian DWORDs:
magic `0x554d534f`, version 1, flags, revision, four reserved. Flag 1 indicates
settings open; flag 2 indicates gameplay active. The controller reads this page
without modifying its contents. Invalid or unstable pages fall back to manual
mode. Native gameplay detection is heuristic; the manual override remains
available for screens whose original state cannot be recognized reliably.

## Verification limits

Unit tests cover Xbox bindings, deadzones, mode selection, chord ordering/hold/
release, and actual Windows mailbox flags/F1 scan codes. The SDL worker has been
smoke-tested with the bundled DLL and no attached devices, without opening the
game or generating OS input. Physical controllers, wireless reconnects and
vendor-specific raw layouts require hardware verification. SDL backend support
is not a claim that every controller model has been tested.
