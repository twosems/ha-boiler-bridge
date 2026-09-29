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
