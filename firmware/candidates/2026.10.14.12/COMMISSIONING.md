# .11 commissioning — NOT_INSTALLED
On current bkcmd2: visible reason, arm disabled, all boiler buttons disabled;
telemetry/MQTT/auth unchanged. On a validated bkcmd0 bench: enable UI only after
fresh diagnostic write_capable and actual switch state. Disconnect/stale diagnostics
must block commands. Never infer actuation from HTTP200. No live actuator tests.
