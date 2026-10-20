# Commissioning .9

NOT_INSTALLED / PHYSICAL_SAFETY_NOT_TESTED. Preserve .8 verified private image.
Before OTA verify actual SHA, target identity, user installation authorization.
After OTA verify boot confirmation, UART RX/TX and losses, controls OFF, MQTT ON.
Observe temperatures through repeated transaction DESYNC and expiry; never force
boiler commands or increase freshness timeout to conceal missing measurements.
Authentication tests first on a test HTTP server/bench: old login fails after
rotation; anonymous access only while disabled; re-enable restores challenge;
protection survives restart. Live auth changes only if operator chooses settings.
No live reboot solely to test persistence. Default existing credentials retained.
No automatic HA changes or physical control operations.
