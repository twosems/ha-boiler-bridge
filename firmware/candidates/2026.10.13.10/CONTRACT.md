# FW 2026.10.13.9 — readings recovery and configurable web authentication

Base: .8 installed image 56022b6f47b78c2c8b0c0bfddb30f08879a951787a191422eff2e241755292c5.
A transient transaction DESYNC clears page trust and request/control state, but
preserves measured values and their original freshness timestamps. Values still
expire after30s. TIMEOUT, UART/mirror gap and startup keep full invalidation.
Transport ownership, command gating and raw UART code unchanged.
New authenticated /boiler-auth endpoint accepts credentials only in POST form
body with required custom header, validates size/username, queues changes to main
loop, persists one configuration record and applies only after preference sync.
GET returns protection state/username/status, never password. Empty new password
keeps previous password. Disabled protection is an explicit UI action; re-enabling
always restores middleware protection including after disabled-state boot.
Auth configuration is independent from MQTT configuration and has a unique key.
Current login/password retained by default; no automatic disabling on installation.
No actuator tests, software compile/host tests do not prove physical safety.
No extra live reboot for persistence testing. Prepare binary and rollback before OTA.
