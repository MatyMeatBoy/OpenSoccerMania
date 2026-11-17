"""Async client shared by VPS, direct IP, and localhost sessions."""
import asyncio
from .protocol import VERSION, receive, send, InputTimeline

class NetplayError(RuntimeError):
    pass

class NetplayClient:
    def __init__(self):
        self.events = asyncio.Queue(maxsize=512)
        self.pending = {}
        self.serial = 0
        self.timeline = InputTimeline()
        self.identity = None

    async def connect(self, host="127.0.0.1", port=27845, ssl=None):
        if getattr(self, "connecting", False) or (hasattr(self, "task") and not self.task.done()):
            raise NetplayError("already connected")
        self.connecting = True
        try:
            self.reader, self.writer = await asyncio.wait_for(asyncio.open_connection(host, port, ssl=ssl), 5)
            self.disconnected = None
            self.events = asyncio.Queue(maxsize=512)
            self.timeline = InputTimeline(self.timeline.capacity)
            self.identity = None
            self.task = asyncio.create_task(self._read())
            try:
                await self.request("hello", version=VERSION)
            except BaseException:
                await self.close()
                raise
            return self
        finally:
            self.connecting = False

    async def _read(self):
        try:
            while True:
                value = await receive(self.reader)
                if "id" in value:
                    future = self.pending.pop(value["id"], None)
                    if future and not future.done():
                        future.set_result(value)
                else:
                    if value.get("event") == "input":
                        self.timeline.accept(value)
                    self.events.put_nowait(value)
        except (Exception, asyncio.CancelledError) as exc:
            for future in self.pending.values():
                if not future.done():
                    future.set_exception(NetplayError("connection lost"))
            self.pending.clear()
            self.disconnected = str(exc)
            self.writer.close()

    async def request(self, op, **fields):
        if getattr(self, "disconnected", None) is not None:
            raise NetplayError("connection lost")
        self.serial += 1
        key = self.serial
        future = asyncio.get_running_loop().create_future()
        self.pending[key] = future
        try:
            await send(self.writer, dict(fields, op=op, id=key))
            try:
                result = await asyncio.wait_for(future, 5)
            except asyncio.TimeoutError as exc:
                raise NetplayError("request timeout") from exc
        finally:
            self.pending.pop(key, None)
        if not result.get("ok"):
            raise NetplayError(result.get("error", "request failed"))
        if "token" in result:
            self.identity = {k: result[k] for k in ("room", "player", "token", "seat")}
        return result

    async def join(self, room, compatibility, name="Player", team=0):
        return await self.request("join", room=room, compatibility=compatibility.wire(), name=name, team=team)

    async def resume(self, identity, compatibility):
        return await self.request("resume", **identity, compatibility=compatibility.wire())

    async def close(self):
        if hasattr(self, "writer"):
            writer, reader_task = self.writer, self.task
            writer.close()
            try:
                await writer.wait_closed()
            except ConnectionError:
                pass
            finally:
                reader_task.cancel()
                await asyncio.gather(reader_task, return_exceptions=True)
