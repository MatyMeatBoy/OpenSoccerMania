"""Bounded in-memory room authority; never executes or injects game input."""
import argparse
import asyncio
import secrets
import ssl
import time
from dataclasses import dataclass, field
from .protocol import VERSION, Compatibility, receive, send

@dataclass
class Player:
    seat: int
    team: int
    name: str
    player: str = field(default_factory=lambda: secrets.token_hex(16))
    token: str = field(default_factory=lambda: secrets.token_urlsafe(32))
    connection: object = None
    expires: float = 0
    ready: bool = False
    last_frame: int = -1

@dataclass
class Room:
    code: str
    compatibility: Compatibility
    players: dict = field(default_factory=dict)
    host: str = ""
    state: str = "lobby"
    session: str = ""
    tokens: float = 480.0
    clock: float = field(default_factory=time.monotonic)

class NetplayServer:
    def __init__(self, *, max_rooms=64, max_connections=128, per_ip=8, lease_seconds=15, idle_seconds=30, rate=180):
        self.rooms = {}
        self.connections = set()
        self.max_rooms, self.max_connections, self.per_ip = max_rooms, max_connections, per_ip
        self.lease_seconds, self.idle_seconds, self.rate = lease_seconds, idle_seconds, rate

    async def start(self, host="127.0.0.1", port=27845, ssl=None):
        self.listener = await asyncio.start_server(self._connection, host, port, ssl=ssl, limit=16384)
        self.port = self.listener.sockets[0].getsockname()[1]
        self.cleaner = asyncio.create_task(self._clean())
        return self

    async def close(self):
        self.listener.close()
        await self.listener.wait_closed()
        self.cleaner.cancel()
        for conn in list(self.connections):
            conn["writer"].close()
        await asyncio.gather(self.cleaner, return_exceptions=True)
        tasks = [c["task"] for c in self.connections]
        if tasks:
            await asyncio.gather(*tasks, return_exceptions=True)

    async def _clean(self):
        while True:
            await asyncio.sleep(0.1)
            for room in list(self.rooms.values()):
                changed = False
                for key, player in list(room.players.items()):
                    if player.connection is None and player.expires <= time.monotonic():
                        del room.players[key]
                        changed = True
                if not room.players:
                    self.rooms.pop(room.code, None)
                elif changed:
                    if room.host not in room.players:
                        room.host = min(room.players.values(), key=lambda p: p.seat).player
                    await self._broadcast(room, self._snapshot(room))

    def _snapshot(self, room):
        return dict(event="room", room=room.code, host=room.host, state=room.state, session=room.session,
                    compatibility=room.compatibility.wire(), engine_integration="pending",
                    players=[dict(player=p.player, seat=p.seat, team=p.team, name=p.name, ready=p.ready,
                                  connected=p.connection is not None) for p in room.players.values()])

    async def _write(self, conn, value):
        async with conn["lock"]:
            await asyncio.wait_for(send(conn["writer"], value), 2)

    async def _broadcast(self, room, value):
        async def deliver(conn):
            try:
                await self._write(conn, value)
            except (ConnectionError, asyncio.TimeoutError):
                conn["writer"].close()
        await asyncio.gather(*(deliver(p.connection) for p in room.players.values() if p.connection))

    async def _connection(self, reader, writer):
        ip = writer.get_extra_info("peername")[0]
        if len(self.connections) >= self.max_connections or sum(c["ip"] == ip for c in self.connections) >= self.per_ip:
            writer.close()
            return
        # Dictionaries are unhashable: identity wrapper keeps live connection references.
        class Connection(dict):
            __hash__ = object.__hash__
            __eq__ = object.__eq__
        conn = Connection(writer=writer, ip=ip, lock=asyncio.Lock(), task=asyncio.current_task(), hello=False,
                          room=None, player=None, tokens=float(self.rate), clock=time.monotonic())
        self.connections.add(conn)
        try:
            while True:
                msg = await asyncio.wait_for(receive(reader), self.idle_seconds)
                now = time.monotonic()
                conn["tokens"] = min(self.rate, conn["tokens"] + (now - conn["clock"]) * self.rate)
                conn["clock"] = now
                if conn["tokens"] < 1:
                    break
                conn["tokens"] -= 1
                key = msg.get("id")
                if type(key) is not int or not 0 < key < 2**53:
                    break
                try:
                    result, event_room = self._handle(conn, msg)
                    await self._write(conn, dict(result, id=key, ok=True))
                    if event_room:
                        await self._broadcast(event_room, self._snapshot(event_room))
                    if msg.get("op") == "input":
                        await self._broadcast(conn["room"], conn.pop("input_event"))
                except (ValueError, KeyError, TypeError) as exc:
                    await self._write(conn, dict(id=key, ok=False, error=str(exc)))
        except (asyncio.IncompleteReadError, ConnectionError, asyncio.TimeoutError, ValueError):
            pass
        finally:
            player, room = conn["player"], conn["room"]
            if player and player.connection is conn:
                player.connection = None
                player.ready = False
                player.expires = time.monotonic() + self.lease_seconds
                if room.state == "started":
                    room.state = "interrupted"  # No simulation/state recovery has been verified.
                await self._broadcast(room, self._snapshot(room))
            self.connections.discard(conn)
            writer.close()
            try:
                await writer.wait_closed()
            except ConnectionError:
                pass

    def _handle(self, conn, msg):
        op = msg.get("op")
        if op == "hello":
            if msg.get("version") != VERSION:
                raise ValueError("protocol mismatch")
            conn["hello"] = True
            return {"version": VERSION, "engine_integration": "pending"}, None
        if not conn["hello"]:
            raise ValueError("hello required")
        if op == "ping":
            return {"nonce": str(msg.get("nonce", ""))[:80]}, None
        if op == "list":
            return {"rooms": [dict(room=r.code, players=len(r.players), state=r.state) for r in self.rooms.values()]}, None
        if op in ("join", "resume"):
            if conn["player"]:
                raise ValueError("already seated")
            code = msg.get("room")
            if not isinstance(code, str) or not 1 <= len(code) <= 32 or not all(c.isalnum() or c in "-_" for c in code):
                raise ValueError("invalid room")
            compatibility = Compatibility.parse(msg["compatibility"])
            room = self.rooms.get(code)
            if room and room.compatibility != compatibility:
                raise ValueError("compatibility mismatch")
            if op == "resume":
                if not room:
                    raise ValueError("session unavailable")
                player = room.players.get(msg.get("player"))
                if not player or not secrets.compare_digest(player.token, str(msg.get("token", ""))):
                    raise ValueError("invalid credentials")
                if player.connection or player.expires <= time.monotonic():
                    raise ValueError("seat unavailable")
            else:
                team = msg.get("team")
                name = msg.get("name")
                if type(team) is not int or team not in (0, 1) or not isinstance(name, str) or not 1 <= len(name) <= 32:
                    raise ValueError("invalid player")
                if not room:
                    if len(self.rooms) >= self.max_rooms:
                        raise ValueError("room capacity")
                    room = self.rooms[code] = Room(code, compatibility)
                if room.state != "lobby":
                    raise ValueError("session already started")
                if len(room.players) >= 4:
                    raise ValueError("room full")
                if sum(p.team == team for p in room.players.values()) >= 2:
                    raise ValueError("team full")
                seat = next(i for i in range(4) if all(p.seat != i for p in room.players.values()))
                player = Player(seat, team, name)
                room.players[player.player] = player
                if not room.host:
                    room.host = player.player
            conn["player"], conn["room"], player.connection = player, room, conn
            return dict(room=code, player=player.player, token=player.token, seat=player.seat), room
        player, room = conn["player"], conn["room"]
        if not player or player.connection is not conn:
            raise ValueError("not seated")
        if op == "ready":
            if room.state != "lobby" or type(msg.get("ready")) is not bool:
                raise ValueError("not in lobby")
            player.ready = msg["ready"]
            return {}, room
        if op == "team":
            team = msg.get("team")
            if room.state != "lobby" or type(team) is not int or team not in (0, 1):
                raise ValueError("invalid team change")
            if sum(p.team == team and p is not player for p in room.players.values()) >= 2:
                raise ValueError("team full")
            player.team, player.ready = team, False
            return {}, room
        if op == "lobby":
            if player.player != room.host:
                raise ValueError("host required")
            if room.state not in ("started", "interrupted"):
                raise ValueError("match is not active")
            if msg.get("session") != room.session:
                raise ValueError("inactive session")
            room.state, room.session = "lobby", ""
            room.tokens, room.clock = 480.0, time.monotonic()
            for member in room.players.values():
                member.ready, member.last_frame = False, -1
            return {}, room
        if op == "start":
            if player.player != room.host:
                raise ValueError("host required")
            if room.state != "lobby" or len(room.players) < 2 or not all(p.ready and p.connection for p in room.players.values()) or {p.team for p in room.players.values()} != {0, 1}:
                raise ValueError("start barrier not satisfied")
            room.state, room.session = "started", secrets.token_hex(16)
            return {"session": room.session, "engine_integration": "pending"}, room
        if op == "input":
            if room.state != "started" or msg.get("session") != room.session:
                raise ValueError("inactive session")
            if set(msg) - {"op", "id", "session", "frame", "input"}:
                raise ValueError("input identity is server assigned")
            frame, value = msg.get("frame"), msg.get("input")
            if type(frame) is not int or not 0 <= frame <= 2**31-1 or frame <= player.last_frame or frame > player.last_frame + 600:
                raise ValueError("stale or out-of-window frame")
            if not isinstance(value, dict) or set(value) != {"x", "y", "buttons"} or any(type(value[k]) is not int for k in value) or not all(-32767 <= value[k] <= 32767 for k in ("x", "y")) or not 0 <= value["buttons"] <= 65535:
                raise ValueError("invalid normalized input")
            now = time.monotonic()
            room.tokens = min(480.0, room.tokens + (now - room.clock) * 480.0)
            room.clock = now
            if room.tokens < 1:
                raise ValueError("room input rate limit")
            room.tokens -= 1
            player.last_frame = frame
            conn["input_event"] = dict(event="input", session=room.session, player=player.player, seat=player.seat, frame=frame, input=value)
            return {}, None
        raise ValueError("unknown operation")

async def _main(args):
    context = None
    if bool(args.cert) != bool(args.key):
        raise SystemExit("--cert and --key must be supplied together")
    if args.cert:
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.load_cert_chain(args.cert, args.key)
    server = await NetplayServer().start(args.bind, args.port, ssl=context)
    print(f"SoccerMania room server listening on {args.bind}:{server.port}; game integration pending", flush=True)
    try:
        async with server.listener:
            await server.listener.serve_forever()
    finally:
        await server.close()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bind", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=27845)
    parser.add_argument("--cert")
    parser.add_argument("--key")
    asyncio.run(_main(parser.parse_args()))

if __name__ == "__main__":
    main()
