# Room server and input transport foundation

This is a working standard-library Python TCP server/client foundation, **not playable online SoccerMania**. The original x86 game's simulation boundary, deterministic input consumption, state snapshots and authority are not verified. No OS key injection, rollback, prediction, snapshot repair, video streaming or native game hook is supplied by this package. A successful `start` opens the transport session only; every response advertises `engine_integration: pending`.

The launcher must not enable online gameplay based on server reachability or matching executable hashes. A future engine adapter must establish frame boundaries, consume the four independent seats, validate deterministic replay or implement host authority/state synchronization, and test real game instances before presenting online play as supported. Matching `Compatibility.engine_bridge` identifies adapters; it does not certify them.

## Run locally, by direct IP, or on a VPS

From the project root with the project's bundled Python runtime (or Python 3.11+):

```powershell
python -m opensoccermania.netplay.server --bind 127.0.0.1 --port 27845
```

Direct host: bind `0.0.0.0`, allow TCP 27845 in the host firewall and forward that TCP port in the router if required. Other clients use the host's reachable IP. LAN clients use its LAN IP. Local clients use `127.0.0.1`. There is no NAT hole punching or external account service. The same client protocol uses the future VPS hostname. Nothing has been deployed.

For an Internet endpoint use `--cert fullchain.pem --key private.key`, and give the client a verifying `ssl.create_default_context()` context. Plain TCP is intended for trusted local testing: without TLS the bearer reconnect token can be intercepted. The server owns no account identity; its random player identifier and reconnect token authenticate only a reserved in-memory seat. Room names are public identifiers, not passwords. No password/private room feature is claimed.

`server/Dockerfile` is built from the repository root: `docker build -f server/Dockerfile -t soccermania-netplay .`. Its default is plain TCP; override its command with certificate/key options and read-only certificate mounts for public service. `soccermania-netplay.service` is a deployment template: create an unprivileged service user, install the package and certificates at the named paths, then adapt Python/path/firewall settings. Neither deployment template has been run on a VPS.

## Client API

```python
from opensoccermania.netplay import Compatibility, NetplayClient

# Inside an asyncio coroutine:
compat = Compatibility.from_file("Game.exe")
client = await NetplayClient().connect("127.0.0.1", 27845)
identity = await client.join("friends", compat, name="Player 1", team=0)
await client.request("ready", ready=True)
# Host only, once at least two players, both teams and everyone ready:
started = await client.request("start")
await client.request("input", session=started["session"], frame=0,
                     input={"x": 0, "y": -32767, "buttons": 1})
# Await client.events.get() for room/input notifications.
# Returns None until all requested seats supplied their exact frame:
inputs = client.timeline.complete(0, [0, 1])
await client.close()
```

The controller/keyboard adapter maps each person's device to their own client/seat and the normalized values. No device is shared over OS keyboard events. Axis values are signed integers -32767..32767; buttons is a 16-bit mask whose action mapping must be agreed by the future engine adapter/rules version. Frames are integer tags supplied by that adapter, not server wall-clock timestamps. A client's event queue holds 512 notifications; applications must consume it continuously or the client disconnects. Timeline retains 240 distinct frames, rejects stale per-seat events and never invents missing input.

Store `client.identity` privately while reconnecting. A new connected client calls `await new_client.resume(identity, compat)` within the default 15-second disconnected-seat lease. Identity/player/seat remain stable. Tokens appear only in that client's join/resume reply, never in room/input broadcasts. An active connection cannot be displaced using a token. Server restart loses every room and token; callers must explicitly create a new lobby, never silently resume an old game. Reconnect recovers the reservation, not game state. A disconnect during a started session changes it to `interrupted` permanently; create another lobby after all old reservations expire. No unsafe resumption is attempted.

## Wire contract and bounds

TCP frames: unsigned 32-bit big-endian byte count followed by UTF-8 JSON object, at most 8192 bytes. Each request includes positive integer `id` and `op`; replies echo `id`, contain `ok`, and an `error` string on rejection. Unsolicited events have `event` and no request ID. `hello` with `version: 1` is required first. Supported operations: `hello`, `ping` (nonce echo), `list`, `join`, `resume`, `team`, `ready`, `start`, `input`. Closing the transport is departure with a reserved-seat lease.

Compatibility is the complete executable SHA-256 plus exact `rules` and `engine_bridge` strings. First join owns the room. Rooms have four unique seats numbered 0..3, maximum two on each team 0/1, no spectators. Only the host can start; at least two connected/ready players and both teams must be present. Team changes clear readiness. After a host lease expires, the lowest remaining seat becomes host. Started/interrupted rooms reject new joins. Input attribution comes from the authenticated connection, never client-supplied player/seat fields. Duplicate/reordered frame tags and jumps over 600 frames from the last accepted frame are rejected. Session IDs prevent old-session inputs.

Defaults: 64 rooms, 128 TCP connections, 8 connections per source IP, 180 requests/second per connection with a 180-request burst, 480 inputs/second per room with a 480-input burst, 30 seconds without a complete request before disconnect, two-second write deadline. Clients should ping at least every ten seconds in idle lobbies. Shared NAT users count together. Slow readers are disconnected. Limits are safety bounds, not measured capacity; no public load-test or Internet latency claim is made. There is no persistence, match history or player-address logging.

## Verification and reference

`python -m unittest discover -s tests -p test_netplay.py -v` runs real loopback TCP clients: four-seat fanout, fifth rejection, team limits, compatibility mismatch, host/ready barrier, stale/spoof rejection, private reconnect credentials, seat leases and interrupted sessions/server outage. The printed elapsed time is aggregate local scripted request/fanout time under the test runner, not a game latency or Internet measurement. TLS deployment, NAT changes, long sessions, multiple machines, engine determinism and human Internet gameplay remain unverified.

Mechanisms were informed by the user-provided MIT-licensed GBANoCable guidance (`local/gbanocable/SKILL.md`, relay and netplay-approach chapters): stamp seats at the server, keep credentials private, bound state/rates, distinguish seat reconnect from game recovery, and select simulation netcode only after inspecting the game. This is an independent Python implementation; no GBA-specific code, constants, ROM or assets were copied.
