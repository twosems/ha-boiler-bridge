# .11 control UI — read-only on current bkcmd2 panel
Base728e114, installed .10. UI consumes existing local1Hz bridge diagnostic JSON;
only actual fresh write_capable/active state enables command controls. Unsupported
bkcmd, unknown/stale diagnostic, readiness/ownership/telemetry failure block arming
and explain reason. HTTP200 never claimed as armed. Transport .10 unchanged except
version string, including bkcmd0 actuation gate. This candidate does not restore
actuation on the current panel. Need separate response-ownership design and bench
validation for bkcmd2. No bkcmd mutation/unknown clicks/live actuator tests.
