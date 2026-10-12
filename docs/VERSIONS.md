# Версии и артефакты

Снимки неизменяемы. Все бинарники находятся внутри `recovery/boiler-originals-20261012.tar.xz.encrypted`; ключ отдельно. Хэши/размеры — фактически прочитанные файлы, не объявление эксплуатационной готовности. Полный машинный реестр: `firmware/versions.json`.

| Snapshot ID | FW label | OTA bytes | OTA SHA-256 | Статус |
|---|---|---:|---|---|
| legacy-base-20260921 | нет метки | 860288 | `45ca85a0f941db17bbc32bfd4e2a49c3e534fa1f6d374f4854a063ee04ab460e` | Legacy baseline, not a verified production rollback |
| legacy-webtest | нет метки | 1075760 | `78473ac7d5026b1b3d5d2a865fa6fbe6c5c1b7b90431027439a3116feb080892` | Historical web/MQTT source; aggressive polling incident reported |
| legacy-webtest-v3 | нет метки | 1075904 | `36b8dfc54d1fcc7d29a85a71a71319d9666c388464562a23df2a40279625f351` | Historical variant; name v3 is not firmware 2026.10.12.3 |
| 2026.10.12.3-rtbridge | 2026.10.12.3 | 1079072 | `f4b3860c52babb8911cd14858db84324a5a1a394a756212256b7a65b255e9bf8` | Build log PASS; passive RT bridge; hardware validation pending |
| 2026.10.12.3-isolated-transport | 2026.10.12.3 | 1074448 | `2f4d681ff86e979e33100c65d03d28e7835fb02e6c147b87345c0258c61d2c24` | Binary + BUILD manifest + generated source match; earlier link failure; final successful log not recovered; hardware validation pending |
| 2026.10.12.1-restart | 2026.10.12.1 | 1076832 | `e99f27b74c0c9d05246dc97604359034bfabd70fb9eade909c5b1e39bdb1a0c3` | OTA accepted historically; wrong home Wi-Fi embedded; not recommended |
| legacy-rollback | нет метки | 1072944 | `7cf6365fc62f3798416594a8939969d65c809a3cdd02491d1b4e8c233119c726` | Prepared historical rollback; device validation not verified |
| 2026.10.12.2-cooperative | 2026.10.12.2 | 1074800 | `76f8e7c12dfdc88057f09f43765dd4577bfa6128c3a45bdc6d0add566dfe390d` | Build log PASS; cooperative UART robustness candidate; not deployed here |

## Правила новых версий

- Firmware: YYYY.MM.DD.N, N увеличивается при любом изменении исполняемого образа. Бинарник со старой меткой не переименовывать в новую версию.
- Полная идентичность: version + snapshot_id/variant + Git commit + config/source hash + binary SHA-256. SSID-профиль фиксировать отдельно, без пароля.
- Integration и card имеют собственные версии; v8 — имя интерфейсной линии, v26/portable15/history1 — cache-resource revisions, а не номера firmware.
- Для каждого кандидата: changelog, разрешённые функции, toolchain/partitions, происхождение, размеры и хэши OTA/factory, отдельные build/test/device статусы, rollback reference.
- После успешного испытания можно создать неизменяемый тег fw-<version>; не помечать .3 production только потому, что файл существует.
- .1: исторически Update Successful 12.10 02:23, затем пропала сеть; домашние Wi-Fi настройки. Повторно не устанавливать без исправленного целевого профиля.
- Два разных .3 — выявленная коллизия: rtbridge (priority18, streams) и isolated-transport (priority20, отдельный класс/queue). Следующий изменённый кандидат должен иметь новый номер.
- Наличие factory/OTA в каталоге не подтверждает соответствие текущему исходнику: перед выпуском сопоставить generated main.cpp, toolchain, журнал и manifest.
