# Архив firmware ESP32 котла

Начать с [контекста](../docs/CONTEXT.md), [реестра версий](../docs/VERSIONS.md) и [восстановления](../docs/RECOVERY.md).

`snapshots/` — восемь неизменяемых снимков исходников. Каждый содержит origin, version label, hashes и original binary references в snapshot.json. Имя каталога не является подтверждением production-готовности.
Исходники с `!secret` требуют своего локального `secrets.yaml`; secrets.example.yaml содержит только placeholders. Архив оригинальных firmware.ota.bin/firmware.factory.bin зашифрован в `../recovery/`.

Оба .3 — пассивные кандидаты без click/get; разные реализации с одинаковой встроенной меткой. Ни один не установлен в рамках этого восстановления.
