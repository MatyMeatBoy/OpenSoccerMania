# Reusable network client connection lifecycle

The socket-level regression reproduces `connection lost` when a closed client
connects again: `_read` records a disconnected marker, but `connect` retains it.
The same object also retains old events, identity and frame-zero timeline state.

1. Keep the red real-socket test: two clients exchange frame zero in a first
   session, close/reconnect, then exchange different frame-zero input in a second
   room. New connection state must not contain old identity/events/frames.
2. Reject connecting an already live client before allocating another socket,
   preserving its original connection. Reset per-connection state only after
   new streams open; keep timeline capacity. Close failed handshakes and always
   stop the reader during close even if stream shutdown reports connection loss.
3. Add active-connection and failed-handshake cleanup controls. Run focused
   network cases, aggregate Python tests and independent review. Document that
   callers retain reconnect credentials separately and new event/timeline
   objects belong to the new connection. Interrupted engine simulation remains
   unimplemented; these checks only prove the room/input transport lifecycle.

Review follow-up: an old `close` could cancel a new reader while awaiting old
stream shutdown. The deterministic gated-shutdown test fails before correction
and passes when writer/task references are captured before awaiting. Also gate
a pending socket open to verify a second connect is rejected without replacement.
