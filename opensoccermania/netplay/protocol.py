"""Versioned, bounded, length-prefixed JSON protocol (TCP)."""
import asyncio
import hashlib
import json
import struct
from dataclasses import dataclass

VERSION = 2
MAX_MESSAGE = 8192

@dataclass(frozen=True)
class Compatibility:
    game_sha256: str
    rules: str = "soccer-input-v1"
    engine_bridge: str = "unverified"

    def wire(self):
        return dict(game_sha256=self.game_sha256, rules=self.rules, engine_bridge=self.engine_bridge)

    @classmethod
    def from_file(cls, path, **kwargs):
        digest = hashlib.sha256()
        with open(path, "rb") as stream:
            for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                digest.update(chunk)
        return cls(digest.hexdigest(), **kwargs)

    @classmethod
    def parse(cls, value):
        if not isinstance(value, dict) or set(value) != {"game_sha256", "rules", "engine_bridge"}:
            raise ValueError("invalid compatibility")
        result = cls(**value)
        if not isinstance(result.game_sha256, str) or len(result.game_sha256) != 64 or any(c not in "0123456789abcdef" for c in result.game_sha256):
            raise ValueError("invalid game fingerprint")
        if any(not isinstance(x, str) or not 1 <= len(x) <= 80 for x in (result.rules, result.engine_bridge)):
            raise ValueError("invalid rules/bridge")
        return result

async def receive(reader):
    length = struct.unpack("!I", await reader.readexactly(4))[0]
    if not 1 <= length <= MAX_MESSAGE:
        raise ValueError("message size")
    value = json.loads(await reader.readexactly(length))
    if not isinstance(value, dict):
        raise ValueError("message must be an object")
    return value

async def send(writer, value):
    data = json.dumps(value, separators=(",", ":"), allow_nan=False).encode()
    if len(data) > MAX_MESSAGE:
        raise ValueError("message size")
    writer.write(struct.pack("!I", len(data)) + data)
    await writer.drain()

class InputTimeline:
    """Bounded confirmed input storage. Missing frames are never predicted implicitly."""
    def __init__(self, capacity=240):
        self.capacity = capacity
        self.frames = {}
        self.latest = {}

    def accept(self, event):
        seat, frame = event["seat"], event["frame"]
        if frame <= self.latest.get(seat, -1):
            return False
        self.latest[seat] = frame
        self.frames.setdefault(frame, {})[seat] = event["input"]
        while len(self.frames) > self.capacity:
            del self.frames[min(self.frames)]
        return True

    def complete(self, frame, seats):
        values = self.frames.get(frame, {})
        return dict(values) if all(seat in values for seat in seats) else None
