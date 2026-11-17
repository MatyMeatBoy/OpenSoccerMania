# Room/input transport lifecycle

This is a four-seat, two-team room and input-relay foundation. It does not yet
synchronize the original game simulation or provide playable online matches.
The server marks engine integration as pending; interrupted games do not resume
simulation automatically. A started room has no verified return-to-lobby flow.

## Reusing a client

`NetplayClient.connect` opens a new connection only when its previous reader has
finished. A live or currently opening connection rejects a second call with
`NetplayError("already connected")` before allocating another stream.

After new streams open, per-connection state is fresh: the disconnected marker
is cleared, identity is unseated (`None`), and new event-queue and timeline
objects replace the old ones. The timeline retains its configured capacity.
Request serials remain monotonic, so delayed cleanup cannot reuse request IDs.
Consumers must read the new `client.events` / `client.timeline` references after
reconnection rather than retaining references from an old connection.

To resume a leased seat, retain a separate copy of `client.identity` before
closing, connect again and call `resume(saved_identity, compatibility)`.
Only a successful server response restores the client's identity. Credentials
remain subject to compatibility, ownership and expiry checks. Joining another
room after reconnect starts with an empty confirmed-input history; frame zero
must contain only that new session's input.

`close` owns the writer and reader it captured at entry. A delayed old stream
shutdown cannot cancel a reader created by a later reconnect. Reader cleanup
also runs if waiting for stream closure raises a connection error. A failed
hello handshake closes its stream and reader so a subsequent attempt can retry.

## Verification

Socket-level tests cover sequential rooms with different frame-zero inputs,
separately saved resume credentials, live and pending duplicate-connect rejection,
failed-handshake cleanup, stream-shutdown errors, and overlapping old close/new
connect. The last race is exercised by gating the old stream's shutdown, opening
a new connection, then releasing the old close and successfully pinging through
the new reader. These checks do not constitute simulation resynchronization,
same-room match restart or WAN/physical-controller acceptance.
