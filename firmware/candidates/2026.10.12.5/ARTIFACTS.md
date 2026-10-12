# FW 2026.10.12.5 — build blocked

Private office candidate. No verified firmware image is available yet. The build host disconnected during compilation; check `BUILD-MANIFEST.json` before resuming. The firmware image is not approved for installation.
`secrets.yaml` stays local and ignored; never publish the office image or local
secrets. Build and hardware status are tracked in `BUILD-MANIFEST.json`.

This profile is passive: it forwards the stock serial link through one transport
owner and exposes observer telemetry. Active reads and boiler controls are disabled.
It is not a full replacement for the currently installed firmware until HA/MQTT
state, sensor freshness, controls, rollback, and UART behavior pass commissioning.
