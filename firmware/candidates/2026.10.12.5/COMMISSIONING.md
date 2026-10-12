# FW .5 commissioning gates

Current state: source candidate only. Do not install on the inline office ESP.

1. Complete ESPHome config/compile. Keep `secrets.yaml` local and ignored; verify
   the office SSID and MQTT restore values without printing credentials.
2. Inspect generated `main.cpp`, UART drivers/ports, task placement, sdkconfig,
   partitions, rollback flags and linker wrappers. Confirm exactly one UART owner
   and that observer failure/overload cannot change physical forwarding.
3. Use two independent 9600 8N1 peers and a logic analyzer. Compare every byte and
   ordering both ways during full duplex, silence, partial Nextion frames, event
   errors, observer overload, Wi-Fi/MQTT churn, and network OTA.
4. Verify UI reports passive mode and does not imply control works. Test MQTT and
   sensor values against the physical screen; do not assume stored parser values
   are fresh.
5. Restore controls only after a tested queue owns every UART write and attributes
   responses to the correct transaction. Do not run burner/feed/fan/reverse/mode
   tests as part of commissioning.
6. Before live OTA, read and hash a complete flash backup, prove the restore route,
   confirm installed bootloader rollback support, and obtain a stable screen/HA/MQTT
   baseline. OTA itself interrupts the inline UART link.

This candidate has not passed any hardware gate. Current office device remains
FW `2026.10.12.1` until all release gates pass.
