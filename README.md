# Boiler Bridge

Custom Home Assistant integration for an ESP32 UART bridge connected to a boiler controller and its stock HMI.

## Features
- current HMI page and raw mode state
- confirmed `Пуск / Работа` state
- confirmed `Поддержка` switch
- safe command serialization and semantic confirmation
- designed to work with **Boiler Control Card**

## Installation with HACS
1. Open HACS → Integrations.
2. Add this repository as a custom repository with category **Integration**.
3. Install **Boiler Bridge**.
4. Restart Home Assistant.
5. Open Settings → Devices & services → Add integration → **Boiler Bridge**.

Current package version: **0.2.5**.

## Firmware archive and conversation recovery

Start with [the recovery guide](docs/RECOVERY.md), [current context](docs/CONTEXT.md), [versions and SHA-256](docs/VERSIONS.md), and [development rules](AGENTS.md).

The 2026-10-12 archive preserves eight firmware source snapshots, 16 original OTA/factory binaries in an encrypted backup, and the supplied conversation. Existing HACS integration code is unchanged. Historical installed integration 0.3.x differs from this repository package 0.2.5; see the context before upgrading. Firmware build success does not imply device validation. Both different 2026.10.12.3 variants are passive candidates; original network profiles require target-network verification.
