# FW 2026.10.12.4 — passive bench candidate

This candidate is NOT approved for unattended deployment or for OTA to the live boiler.
Base Git: cc642c0ed76f6697ed542a9bab470314718da7c2.

- Classic dual-core ESP32 only. RX16/TX17 is display→controller; RX18/TX19 controller→display. 9600 8N1. UART0 belongs to the USB logger.
- Own IDF UART1/UART2 drivers with RX event queues; IDF TX buffer is zero, bounded software rings feed uart_tx_chars. No ESPHome UART component owns these ports.
- Static bridge/supervisor tasks and buffers. Observer is optional and never controls forwarding. No command insertion, click/get, legacy active fallback, actuator buttons, restart endpoint or ultrasonic sensor.
- Bridge initializes before network. Standard safe_mode is disabled; no network-only recovery mode can replace this transport. Startup failure is explicitly fatal/degraded, never an active downgrade.
- Main loop unsubscribes from fatal TWDT only after driver+task+bridge-WDT readiness and supervisor creation are confirmed. IWDT and bridge TWDT remain. Main liveness is diagnostic only.
- ESPHome HAL feeds unconditionally: a linker wrapper skips feeds only for the exact detached loop task, avoiding IDF task-not-found log floods. Every other task still calls the real watchdog reset API.
- Supervisor separates normal silent idle from pending-TX stall and stalled bridge task. Irrecoverable transport/task faults reset the ESP as LAST RESORT; reset interrupts UART and is not hardware bypass.
- UART FIFO loss is counted and invalidates observer/boot validation. A UART_BUFFER_FULL warning alone is not claimed as proven loss. Frame/parity events do not autonomously reset a healthy forwarding task.
- Mirror blocks carry sequence, epoch and capture time. Gap detection precedes decoding; stale/missing data never becomes zero. Observer here is a deliberately minimal stock-protocol observer, not full parity with the old UI.
- Single boot coordinator authorizes the OTA valid operation. Linker wrapping blocks any other auto-confirm. Pending image is confirmed only after 60 s with both-direction traffic, no UART errors, complete enqueue of observed bytes and a healthy transport. Silence leaves WAIT_TRAFFIC, not false GOOD/reboot. Installed bootloader support and useful previous image MUST be checked before deployment.
- No compiled Wi-Fi SSID/password: USB Improv provisioning. This changes ESPHome's saved-Wi-Fi preference key relative to configured-SSID images; office Wi-Fi migration is NOT assumed. No automatic Setup AP. Credentials in the bench build are public placeholders; replacing credentials produces a new artifact/hash.
- ESPHome OTA is password-protected but maintenance-only; HTTP web OTA is absent. OTA writes may interrupt forwarding; no IRAM/lossless guarantee. No firmware signing/unattended update guarantee is claimed.
- Timeouts are bench values, not validated boiler-protocol limits: bridge task 250 ms; pending TX no-progress 500 ms; backlog warning 256 bytes; fatal age 2000 ms. Target-device acceptance requires a serial-peer/logic-analyzer bench and assessment of these limits.

## Reduced commissioning scope

The expedited first artifact exposes only UART diagnostics and unsolicited PAGE.
Setpoints, modes, MQTT and the former control UI are not included; this is not a
drop-in full-feature replacement. GET response attribution is disabled rather
than guessing ownership. The stock physical display still talks to the boiler.
The local ESPHome diagnostic web page is not a redesign of the production UI.
`allow_boot_confirm: false` deliberately prevents marking this bench image GOOD;
it must not be sent by unattended OTA. USB factory commissioning is required.
The conditional confirmation code is reserved for a later validated contract.
OTA maintenance is limited to 120 seconds and does not promise lossless UART.

The independent hopper sensor belongs to a different device/project. It is not part of this firmware.
