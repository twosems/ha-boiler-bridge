# FW 2026.10.12.5 — office integration candidate

Status: local ESPHome/ESP-IDF build PASS. It is not hardware-tested or approved
for installation on the live inline ESP.

## Current candidate behavior

- Target: classic dual-core ESP32, display RX16/TX17, boiler RX18/TX19, 9600 8N1.
- BoilerTransportV4 owns the IDF UART1/UART2 drivers and is the only physical
  UART reader/writer. Static bridge/supervisor tasks, bounded transport rings,
  optional observer mirror, UART event/error and TX progress checks are inherited
  from the separately built `.4` source.
- The old `boiler_proxy` parser consumes only observer copies from the transport.
  It has no UART driver IDs. Passive mode is forced by the dedicated transport
  path; legacy requests to refresh or click are rejected. No fallback to the old
  parser-first bridge is allowed.
- Legacy entity and MQTT YAML is carried forward. Since active `get` requests are
  disabled, sensor freshness and availability of unsolicited values are not yet
  demonstrated. UI/HA controls do not work in this passive candidate and must not
  be presented as functional.
- Wi-Fi credentials are loaded from ignored local `secrets.yaml`; office SSID is
  `2Soffice`. The user supplied the passphrase, which is not stored in Git. Wi-Fi
  loss does not reboot the bridge (`reboot_timeout: 0s`); a fallback AP is present.
- API encryption key is recovered locally from the exact archived `.1` profile.
  OTA password and fallback AP password are unique local secrets. MQTT settings
  rely on the existing ESPHome restore-value preferences; migration must be
  confirmed on the target image before deployment.
- ESPHome `safe_mode` is disabled because its recovery mode can stop the inline
  UART bridge. IDF rollback is explicitly enabled in the custom component; only
  the transport supervisor may confirm a pending image, and only after bridge
  startup plus observed traffic in both directions. If traffic is absent the
  image remains pending. Installed bootloader rollback support is unknown.
- OTA can interrupt UART traffic and is not lossless. No claim of continued bridge
  service during flash/reset/power loss is made.

## Release gates

1. Finish source integration and structural checks; ensure generated code has one
   UART owner, no second writer, no active injections, and no legacy fallback.
2. Compile with ESPHome/IDF and inspect generated C++, sdkconfig, partitions,
   rollback and linker wrappers; record binary hashes and private network profile.
3. On a separate two-peer serial bench, prove byte-for-byte forwarding under
   full duplex and observer/parser overload; verify fault and OTA behavior.
4. Decide and implement safe transaction ownership before restoring controls or
   active GET polling. Until then controls remain disabled.
5. Confirm flash backup/restore route, bootloader rollback, saved MQTT preference
   migration, screen operation, MQTT and sensor freshness before office OTA.

`BUILD_PASS`, if later achieved, will not mean `BENCH_PASS` or `OFFICE_READY`.
The hopper sensor belongs to its own independent ESP and is out of scope.
