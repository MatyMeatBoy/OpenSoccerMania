import asyncio
import time
import unittest
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
        await clients[1].close()
        await asyncio.sleep(0.03)
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
        await clients[0].close()
        await asyncio.sleep(0.04)
        self.assertEqual(self.server.rooms["test"].state, "interrupted")
        with self.assertRaisesRegex(NetplayError, "inactive"):
            await clients[1].request("input", session=session, frame=0, input={"x":0,"y":0,"buttons":0})
        await asyncio.sleep(0.3)
        self.assertNotEqual(self.server.rooms["test"].host, clients[0].identity["player"])
        await self.server.close()
        # server.close is idempotent enough for fixture teardown; no old sessions survive restart.
        await asyncio.sleep(0.01)
        with self.assertRaises((NetplayError, ConnectionError)):
            await clients[1].request("ping")

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
