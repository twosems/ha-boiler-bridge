# Commissioning 2026.10.13.8

NOT DEPLOYED / HARDWARE NOT TESTED. Update only with a physical observer and a user command. Keep USB recovery, the saved full-flash backup and the .6 package.

1. Verify actual OTA image SHA-256 and current device identity/network before uploading.
2. After update confirm .7 returns, bridge RX/TX progress, pending=0 and no UART losses/errors.
3. Confirm panel_bkcmd, read-only status and actual PAGE. Do not press boiler controls.
4. Compare BTEMP/DTEMP/setpoints with the physical panel on page1; do not call synthetic test values physical evidence. Verify data becomes unknown when stale.
5. Navigate physically and confirm no read applied to the wrong page. Read rejection must not change boiler settings or stop stock traffic.
6. MQTT auth remains unresolved; verify before modifying settings. Do not reset HA.

If telemetry fails, preserve raw diagnostics and physical observations. Do not force bkcmd=0 or repeatedly enable actuation. USB restore resets the inline bridge and requires an observer.
