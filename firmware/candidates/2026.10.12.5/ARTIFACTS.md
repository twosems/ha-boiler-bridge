# FW 2026.10.12.5 — locally built office candidate

ESPHome 2026.9.0 / ESP-IDF 5.5.5 compiled successfully in the current isolated workspace. The generated source matches the candidate component byte-for-byte; the build has explicit IDF rollback and no ESPHome safe-mode early return. See `BUILD-MANIFEST.json` for resource use and SHA-256 values.

The OTA and factory images are stored only in the current private workspace output folder. They include compiled office Wi-Fi/API/OTA credentials and are deliberately excluded from this public GitHub repository. The repository contains source and non-secret hashes/status only.

This is a passive candidate: HA boiler controls and active sensor GET requests are disabled. It has not passed a two-peer UART bench, hardware fault injection, office preference migration, or device commissioning. `BUILD_PASS` does not mean `OFFICE_READY`; do not install it on the live inline ESP as a full replacement.
