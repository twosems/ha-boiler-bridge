# FW 2026.10.13.8 — independent read-only telemetry

Classic ESP32-WROOM-32, 9600 8N1, original GPIO16/17/18/19 unchanged.

Only the RT transport owns UART. Read-only GET/sendme is enabled independently of actuation. Panel bkcmd values 0..3 are validated without changing bkcmd. CLICK remains OFF after boot, requires explicit permission and verified bkcmd=0. Invalid responses, ownership loss, UART errors, timeouts, expiry and maintenance disable injection. Stock bytes and late-own-response handling keep the .6 transport contract.

Reading compatibility and physical-device tests are pending. A successful build is not hardware PASS. UART can be interrupted during OTA/NVS writes or reset; no hardware bypass is installed. Freshness remains 30 seconds. Temperatures depend on the actual visible panel page and supported components; unknown values are not zero.

MQTT connection/authentication is independent of web readings. Do not publish secrets.yaml or raw credential-bearing binaries to public Git. Retain the .6 image and exact 4 MB backup; neither is a proven full-operational rollback.

Details: docs/READINGS-FIX-20261013.md.
