# Host-controlled room return and fresh sessions

The existing transport cannot return a started/interrupted room to its lobby.
The real-socket red test receives unknown operation. Complete this lifecycle
without claiming playable game synchronization.

1. Add host-only `lobby` for started/interrupted rooms, requiring its matching
   session ID so delayed commands cannot stop a later round. Invalidate session,
   clear readiness and per-player last frame, reset room relay budget/clock.
   Preserve seats, credentials and disconnected seat leases. Require the normal
   full ready/connected/two-team barrier again before starting.
2. Clients track active session from authoritative room snapshots. Changing or
   ending a session clears confirmed timeline and queued input events while
   preserving the event-queue object and noninput notices. Reject relayed input
   without a matching active session. Disconnect also ends the local session.
3. Bump transport VERSION to 2: older clients lack required timeline lifecycle
   semantics and must fail handshake instead of participating silently.
4. Real-socket tests cover two starts in one four-seat room, readiness resets,
   host-only control, interruption/resume barrier, delayed old relay rejection
   and successful new frame 0/1. Run focused and aggregate tests and review.
