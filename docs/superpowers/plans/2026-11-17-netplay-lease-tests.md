# Deterministic reconnect lease coverage

Evidence: concurrent Python run in `postrelease-pad-python.log` fails valid
resume after the 200 ms fixture lease expires and cleanup removes its player.
Production lease checks are correct for that outcome; the success test's wall
clock dependency chooses an unintended case. Do not lengthen production leases.

1. Give sensitive tests a controlled server-module time object, leaving asyncio
   and client wall clocks untouched. Activate it after setup/join operations.
2. Wait for actual server disconnection/cleanup states with bounded async waits,
   replacing fixed sleeps that assume an event-loop scheduling order.
3. Keep successful authenticated reconnect independent of elapsed wall time.
   Add socket-level just-before/exactly-at/after-deadline checks; stop periodic
   cleanup in that one case to isolate request-time deadline enforcement.
4. Keep a separate active-cleaner expiration test verifying interruption,
   membership removal and host reassignment after clock advancement.
5. Run focused network tests and full Python suite; review test coverage and
   preserve the original failed log. No production networking behavior changes.
