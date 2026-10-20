# .14 web command-handler fix — NOT_INSTALLED
Base: exact .13 build source archive SHA2673dd6fdb29d9f474e9a794a56a92bfb7ac357cca7d608435b563482aa8e5cc.
Missing data-btn click listener in .13 reproduced in actual JS VM.
Fix: one delegated click listener attached to persistent root; press(name,element)
retains existing guards and actuator confirmation. Root DOM is not recreated
during pointer/keyboard activation; fresh safety indicators still update.
No duplicate listeners on render; no automatic retry or command replay.
Only transport source difference is firmware version string. YAML version only.
No UART/GPIO/buffer/task/watchdog/transaction/queue/TTL/ownership/fence changes.
MQTT entity names unchanged. User confirms physical screen remained main after
attempts. Live .13 control armed, click_id/stage0; consistent with missing handler.
Software test does not prove actual Nextion navigation/actuator behavior.
Inherited .13 PAGE/marker/press-release assumptions remain unverified on real HMI.
No actual button requests sent by these tests.
