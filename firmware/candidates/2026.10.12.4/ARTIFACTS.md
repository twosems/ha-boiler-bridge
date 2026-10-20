# .4 BUILD_PASS — только стенд

Source commit: `f801357f21ef7aa6aaa813096da99b903ed606a4`.
Бинарники: `artifacts/boiler-2026.10.12.4-BENCH.factory.bin` и
`artifacts/boiler-2026.10.12.4-BENCH.ota.bin`.
См. BUILD-MANIFEST.json для SHA-256, точного toolchain и результатов generated checks.

Factory — объединённый USB-образ classic ESP32, файл начинается с offset 0.
OTA/app сохранён для воспроизводимости, **не разрешён для unattended установки**:
этот профиль намеренно не подтверждает pending image GOOD.

API/OTA ключи публичные тестовые. SSID не зашит, требуется USB provisioning.
Это не офисный production образ, не замена полного Web/MQTT управления.
Выставление нового приватного deployment профиля даёт другой бинарник и hash.
Перед живым ESP: полный backup, проверенная цель/пины/сеть/recovery и стендовый PASS.
См. COMMISSIONING.md. Сама компиляция не доказывает исправность UART на котле.
