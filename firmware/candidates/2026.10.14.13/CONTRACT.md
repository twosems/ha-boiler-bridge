# .12 guarded control candidate — NOT_INSTALLED
Base .11 source, active device .10. New transport CLICK support for panel bkcmd2
uses serial START/END markers emitted by printh. Never assigns bkcmd. bkcmd0 keeps
its existing contract;1/3 unsupported for actuation,0..3 remain readable.
Explicit arming is OFF at boot; bkcmd2 must complete a read-only marker probe before
any click is admitted. An unstarted arm request expires3s. Every CLICK re-reads bkcmd,
verifies current page, has TTL1500ms, rejects intervening stock traffic, injects one
contiguous press/release pair and never retries. Exact END gives SENT_UNCONFIRMED,
not proof of a relay or boiler setting. Real telemetry is needed for confirmation.
Missing/invalid markers or error disables commands; timed-out fence stays a tombstone
until exact END/reset, so late replies cannot trigger a new command. Failed probes
may pause active telemetry queries; RAW forwarding remains. Real HMI compatibility
and event handlers not yet verified. Do not label this production-safe from host tests.
Retained/DUP command MQTT messages rejected before subscription callbacks, including
fragmented messages; retained states/availability/Discovery unaffected. WiFi/MQTT
loss, maintenance, transport fault and main-loop stall disarm commands. Operator
must re-arm, no reconnect auto-replay. Sensor/queue data, UART GPIO/9600/driver/task/
watchdog/OTA confirmation logic unchanged. Classification temporarily buffers screen
frames during owned transactions; forwarding outside these windows remains stock.
