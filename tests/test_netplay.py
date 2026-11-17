import asyncio
import time
import unittest
from types import SimpleNamespace
from unittest.mock import patch
from opensoccermania.netplay import Compatibility, InputTimeline, NetplayClient, NetplayError, NetplayServer

COMPAT = Compatibility("a" * 64)

class NetworkTests(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        self.server = await NetplayServer(lease_seconds=0.2).start(port=0)
        self.clients = []

    async def asyncTearDown(self):
        for client in self.clients:
            await client.close()
        await self.server.close()

    def control_server_clock(self):
        # Replace this module's time reference, never asyncio's monotonic clock.
        clock = [time.monotonic()]
        override = patch("opensoccermania.netplay.server.time",
                         SimpleNamespace(monotonic=lambda: clock[0]))
        override.start()
        self.addCleanup(override.stop)
        return clock

    async def wait_until(self, predicate):
        async def observed():
            while not predicate():
                await asyncio.sleep(0.005)
        await asyncio.wait_for(observed(), 5)

    async def client(self):
        client = await NetplayClient().connect(port=self.server.port)
        self.clients.append(client)
        return client

    async def players(self):
        clients = [await self.client() for _ in range(4)]
        for i, client in enumerate(clients):
            await client.join("test", COMPAT, team=i // 2)
        return clients

    async def test_four_clients_barrier_inputs_and_limits(self):
        clients = await self.players()
        fifth = await self.client()
        with self.assertRaisesRegex(NetplayError, "room full"):
            await fifth.join("test", COMPAT)
        with self.assertRaisesRegex(NetplayError, "host required"):
            await clients[1].request("start")
        with self.assertRaisesRegex(NetplayError, "barrier"):
            await clients[0].request("start")
        for client in clients:
            await client.request("ready", ready=True)
        session = (await clients[0].request("start"))["session"]
        start = time.perf_counter()
        for client in clients:
            await client.request("input", session=session, frame=0, input={"x": 0, "y": -32767, "buttons": 1})
        for _ in range(100):
            if all(c.timeline.complete(0, range(4)) for c in clients):
                break
            await asyncio.sleep(0.001)
        self.assertTrue(all(c.timeline.complete(0, range(4)) for c in clients))
        print(f"loopback four-client input relay elapsed: {(time.perf_counter()-start)*1000:.2f} ms")
        with self.assertRaisesRegex(NetplayError, "stale"):
            await clients[0].request("input", session=session, frame=0, input={"x":0,"y":0,"buttons":0})
        with self.assertRaisesRegex(NetplayError, "identity"):
            await clients[0].request("input", session=session, frame=1, seat=2, input={"x":0,"y":0,"buttons":0})

    async def test_teams_fingerprint_and_reconnect_secret(self):
        clients = await self.players()
        with self.assertRaisesRegex(NetplayError, "team full"):
            await clients[2].request("team", team=0)
        fifth = await self.client()
        with self.assertRaisesRegex(NetplayError, "compatibility"):
            await fifth.join("test", Compatibility("b"*64))
        identity = clients[1].identity.copy()
        room = self.server.rooms["test"]
        self.assertNotIn(identity["token"], str(self.server._snapshot(room)))
        clock = self.control_server_clock()
        player = room.players[identity["player"]]
        await clients[1].close()
        await self.wait_until(lambda: player.connection is None)
        self.assertEqual(player.expires, clock[0] + self.server.lease_seconds)
        # A wall-clock delay beyond the fixture lease must not change this case.
        await asyncio.sleep(self.server.lease_seconds + 0.05)
        bad = dict(identity, token="wrong")
        with self.assertRaisesRegex(NetplayError, "credentials"):
            await fifth.resume(bad, COMPAT)
        result = await fifth.resume(identity, COMPAT)
        self.assertEqual(result["seat"], identity["seat"])
        self.assertEqual(result["player"], identity["player"])

    async def test_outage_interrupts_and_lease_expires(self):
        clients = await self.players()
        for client in clients:
            await client.request("ready", ready=True)
        session = (await clients[0].request("start"))["session"]
        clock = self.control_server_clock()
        room = self.server.rooms["test"]
        departed = room.players[clients[0].identity["player"]]
        await clients[0].close()
        await self.wait_until(lambda: departed.connection is None)
        self.assertEqual(room.state, "interrupted")
        with self.assertRaisesRegex(NetplayError, "inactive"):
            await clients[1].request("input", session=session, frame=0, input={"x":0,"y":0,"buttons":0})
        clock[0] = departed.expires
        await self.wait_until(lambda: departed.player not in room.players)
        self.assertNotEqual(room.host, departed.player)
        await self.server.close()
        # server.close is idempotent enough for fixture teardown; no old sessions survive restart.
        with self.assertRaises((NetplayError, ConnectionError)):
            await clients[1].request("ping")

    async def test_closed_client_reconnect_starts_with_clean_session_state(self):
        pair = [await self.client() for _ in range(2)]
        for seat, client in enumerate(pair):
            await client.join("first", COMPAT, team=seat)
            await client.request("ready", ready=True)
        first_session = (await pair[0].request("start"))["session"]
        for client in pair:
            await client.request("input", session=first_session, frame=0,
                                 input={"x": 123, "y": 0, "buttons": 1})
        await self.wait_until(lambda: all(c.timeline.complete(0, range(2)) for c in pair))
        pair[0].timeline.capacity = 3
        for client in pair:
            self.assertFalse(client.events.empty())
            capacity = client.timeline.capacity
            await client.close()
            await client.connect(port=self.server.port)
            self.assertEqual(client.timeline.capacity, capacity)
            self.assertIsNone(client.identity)
            self.assertEqual(client.timeline.frames, {})
            self.assertEqual(client.timeline.latest, {})
            self.assertTrue(client.events.empty())
        for seat, client in enumerate(pair):
            await client.join("second", COMPAT, team=seat)
            await client.request("ready", ready=True)
        second_session = (await pair[0].request("start"))["session"]
        self.assertNotEqual(second_session, first_session)
        for client in pair:
            await client.request("input", session=second_session, frame=0,
                                 input={"x": -321, "y": 0, "buttons": 2})
        await self.wait_until(lambda: all(c.timeline.complete(0, range(2)) for c in pair))
        for client in pair:
            self.assertEqual(client.timeline.complete(0, range(2)),
                             {seat: {"x": -321, "y": 0, "buttons": 2} for seat in range(2)})

    async def test_connect_rejects_live_client_without_replacing_connection(self):
        client = await self.client()
        writer, reader_task = client.writer, client.task
        with self.assertRaisesRegex(NetplayError, "already connected"):
            await client.connect(port=self.server.port)
        self.assertIs(client.writer, writer)
        self.assertIs(client.task, reader_task)
        self.assertEqual((await client.request("ping", nonce="still-live"))["nonce"], "still-live")

    async def test_connect_rejects_second_call_while_socket_is_opening(self):
        client = NetplayClient()
        self.clients.append(client)
        started, release = asyncio.Event(), asyncio.Event()
        open_connection = asyncio.open_connection
        async def gated_open(*args, **kwargs):
            started.set()
            await release.wait()
            return await open_connection(*args, **kwargs)
        with patch("opensoccermania.netplay.client.asyncio.open_connection", new=gated_open):
            first = asyncio.create_task(client.connect(port=self.server.port))
            try:
                await asyncio.wait_for(started.wait(), 1)
                with self.assertRaisesRegex(NetplayError, "already connected"):
                    await asyncio.wait_for(client.connect(port=self.server.port), 1)
            finally:
                release.set()
                await first
        self.assertEqual((await client.request("ping", nonce="only-one"))["nonce"], "only-one")

    async def test_old_close_cannot_cancel_reconnected_reader(self):
        client = await self.client()
        old_writer, old_reader = client.writer, client.task
        wait_closed = old_writer.wait_closed
        started, release = asyncio.Event(), asyncio.Event()
        async def gated_shutdown():
            await wait_closed()
            started.set()
            await release.wait()
        with patch.object(old_writer, "wait_closed", new=gated_shutdown):
            closing = asyncio.create_task(client.close())
            try:
                await asyncio.wait_for(started.wait(), 1)
                await self.wait_until(old_reader.done)
                await client.connect(port=self.server.port)
                new_reader = client.task
                self.assertIsNot(new_reader, old_reader)
            finally:
                release.set()
                await closing
        self.assertFalse(new_reader.done())
        self.assertEqual((await client.request("ping", nonce="new-survives"))["nonce"], "new-survives")

    async def test_failed_handshake_closes_stream_and_reader(self):
        client = NetplayClient()
        self.clients.append(client)
        from unittest.mock import AsyncMock
        with patch.object(client, "request", new=AsyncMock(side_effect=NetplayError("handshake rejected"))):
            with self.assertRaisesRegex(NetplayError, "handshake rejected"):
                await client.connect(port=self.server.port)
        self.assertTrue(client.writer.is_closing())
        self.assertTrue(client.task.done())
        await client.connect(port=self.server.port)
        self.assertEqual((await client.request("ping", nonce="retry"))["nonce"], "retry")

    async def test_reused_client_resumes_with_separately_saved_credentials(self):
        client = await self.client()
        await client.join("resume", COMPAT)
        identity = client.identity.copy()
        self.control_server_clock()
        player = self.server.rooms["resume"].players[identity["player"]]
        await client.close()
        await self.wait_until(lambda: player.connection is None)
        await client.connect(port=self.server.port)
        self.assertIsNone(client.identity)
        await client.resume(identity, COMPAT)
        self.assertEqual(client.identity, identity)

    async def test_close_stops_reader_after_stream_shutdown_error(self):
        client = await self.client()
        from unittest.mock import AsyncMock
        with patch.object(client.writer, "wait_closed", new=AsyncMock(side_effect=ConnectionResetError)):
            await client.close()
        self.assertTrue(client.writer.is_closing())
        self.assertTrue(client.task.done())

    async def test_resume_lease_deadline_is_exclusive(self):
        clients = await self.players()
        replacement = await self.client()
        clock = self.control_server_clock()
        room = self.server.rooms["test"]
        identity = clients[1].identity.copy()
        player = room.players[identity["player"]]
        # Isolate request-time expiry from periodic removal; the active-cleaner
        # test separately checks removal and host reassignment.
        self.server.cleaner.cancel()
        await asyncio.gather(self.server.cleaner, return_exceptions=True)
        await clients[1].close()
        await self.wait_until(lambda: player.connection is None)
        clock[0] = player.expires - 0.001
        result = await replacement.resume(identity, COMPAT)
        self.assertEqual(result["seat"], identity["seat"])
        self.assertEqual(result["player"], identity["player"])
        await replacement.close()
        await self.wait_until(lambda: player.connection is None)
        candidate = await self.client()
        clock[0] = player.expires
        with self.assertRaisesRegex(NetplayError, "seat unavailable"):
            await candidate.resume(identity, COMPAT)
        clock[0] += 1
        with self.assertRaisesRegex(NetplayError, "seat unavailable"):
            await candidate.resume(identity, COMPAT)
        self.assertIs(room.players[player.player], player)

    async def test_protocol_bounds_and_room_capacity(self):
        import struct
        reader, writer = await asyncio.open_connection("127.0.0.1", self.server.port)
        writer.write(struct.pack("!I", 8193))
        await writer.drain()
        self.assertEqual(await asyncio.wait_for(reader.read(1), 1), b"")
        writer.close()
        await writer.wait_closed()
        first = await self.client()
        with self.assertRaisesRegex(NetplayError, "protocol mismatch"):
            await first.request("hello", version=999)
        self.server.max_rooms = 1
        await first.join("one", COMPAT)
        other = await self.client()
        with self.assertRaisesRegex(NetplayError, "room capacity"):
            await other.join("two", COMPAT)
        with self.assertRaises(NetplayError):
            await other.request("join", room="bad", compatibility={"game_sha256": [], "rules":"x", "engine_bridge":"x"}, name="x", team=0)

    async def test_connection_rate_bound(self):
        # Separate server with a one-request burst: hello consumes it.
        limited = await NetplayServer(rate=1).start(port=0)
        client = await NetplayClient().connect(port=limited.port)
        try:
            with self.assertRaises(NetplayError):
                await client.request("ping")
        finally:
            await client.close()
            await limited.close()

    def test_timeline_bounded_no_prediction(self):
        timeline = InputTimeline(2)
        for frame in range(3):
            self.assertTrue(timeline.accept(dict(seat=0, frame=frame, input={})))
        self.assertEqual(len(timeline.frames), 2)
        self.assertFalse(timeline.accept(dict(seat=0, frame=1, input={})))
        self.assertIsNone(timeline.complete(2, [0, 1]))

if __name__ == "__main__":
    unittest.main()
