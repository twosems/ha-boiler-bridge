# .14 commissioning — explicit install instruction required
After install boot controlsOFF; verify firmware14, OTA state2, UART counters,
readings, mirror drops and read retries before any command test.
Under physical observation, arm marker check and agree one navigation-only test.
Observe QUEUED and click_id/stage/result separately from local button animation;
SENT_UNCONFIRMED still requires actual physical page change confirmation.
Do not retry a cancelled/expired click automatically. Do not test actuators/modes.
Inherited .13 transport limitations and existing ownership_fault lock require
separate work; this GUI-only change does not fix telemetry ownership recovery.
Rollback .13 private office-pc boiler13/firmware.ota.bin SHA57351af6b90619186c60d4db4ff13e5755d2a63280a804a61275878e9a8712cf.
