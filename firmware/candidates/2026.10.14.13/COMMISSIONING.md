# .12 commissioning — NOT_INSTALLED / BENCH_PENDING
Before any installation: report final source/hash/diff/software-test limits and exact
.10 rollback. Obtain explicit installation instruction; no automatic actuator tests.
Preferred bench: real HMI/project, disconnected actuators; verify printh markers,
stock packets/ACK/errors preservation and page-change event behavior on press/release.
Host tests alone do not validate these real behaviors. No physical bench currently
available to the assistant. Live device .10 is unchanged during development.
After authorized installation: boot/readings/UART first, controlsOFF; exact marker
probe only under operator observation, no boiler clicks. Check no markers/errors
reach boiler, counters/loss0, arm result consistent. Navigation/non-actuator test
requires agreed concrete action and physical observation; actuator commands separate.
Unsuccessful probe may lock active queries until its exact marker/reset, never auto
reset a live boiler simply to clear it. Do not replay expired or rejected commands.
