# .15 acceptance before/after installation

NOT INSTALLED. Do not claim HMI navigation/actuators verified by software tests.

1. Verify running .14 and local rollback image. Confirm physical screen works. Single OTA only after installation authorization.
2. Observe boot_sync progress0->1->2, ownership_fault=false, telemetry_enabled=true, new finite temperature states, write_capable=false, active OFF. Verify UART counters and exact RX/TX accounting with own injected/suppressed bytes included; error and mirror-loss counters zero.
3. If synchronization fails: retain first reason/time/detail, stock error/discard counters. Do not force clear or restart/reflash in a loop. Runtime ambiguity remains fail closed. No automatic mode/fan/auger test.

Bounded host tests exercise actual transactions.cpp with mocked UART/RTOS adapters, including all startup cuts and pre-START stock replies. They do not replace physical validation.
