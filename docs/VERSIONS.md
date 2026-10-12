# Обновление 2026-10-12: FW 2026.10.12.6

Полный кандидат собран: изолированный UART мост, защищённые активные запросы/кнопки, телеметрия, Web/MQTT/HA, журнал смены Wi-Fi, ручной Setup. Проверки host transport/network/web прошли; аппаратура не проверена, ACTIVE_REQUESTS=OFF. Текущая установленная версия неизвестна после неудачной OTA. Следующий этап: USB backup-first и проверка в офисе 13 октября. Контракт/ограничения/артефакты: firmware/candidates/2026.10.12.6/. Секреты не публикуются.

# Версии и артефакты

## Текущий кандидат .5 (2026-10-12)

`2026.10.12.5` / `firmware/candidates/2026.10.12.5` / ветка
`feature/boiler-fw-2026-10-12-5`: private office profile for `2Soffice`, local
ignored secrets. ESPHome 2026.9.0 / ESP-IDF 5.5.5; BUILD_PASS locally. OTA
1066816 bytes, SHA256 `b92c7feb47d71f0af486df62e5f20d70efd7ada65a1636fa25629970b422f1f7`.
Passive observer only; full active GET and boiler controls disabled. Hardware NOT
TESTED, OFFICE_READY=false. OTA was attempted; POST `/update` returned HTTP 408.
Device is currently UNCONFIRMED/UNREACHABLE by HTTP and OTA at `.172`; IP scanner
still identifies `boiler-proxy` there. Do not report installed version or repeat
OTA until connectivity and firmware status are verified. Recovery `.1` was staged
on office-pc (SHA256 `e99f27b74c0c9d05246dc97604359034bfabd70fb9eade909c5b1e39bdb1a0c3`),
but contains the historical home Wi-Fi profile. `.5` is not a complete replacement
for `.1`.
Do not publish private OTA/factory binaries because they include office credentials.
See candidate `BUILD-MANIFEST.json`, `CONTRACT.md`, and `COMMISSIONING.md`.

## Новый кандидат .4

`2026.10.12.4` / `firmware/candidates/2026.10.12.4` / ветка
`feature/boiler-fw-2026-10-12-4`: PASSIVE BENCH_ONLY, source/core checks PASS,
ESP32 BUILD_PASS + generated checks PASS, hardware NOT TESTED, NOT DEPLOYED.
Source SHA `f801357f21ef7aa6aaa813096da99b903ed606a4`; финальный job_8a7cc4fe3ffb.
OTA 912384 bytes / `0f8902261d0098a6bc53aecd1a261225c09a5082b10b065644f76b5673cf6e70`.
Factory 977920 bytes / `49b332333e49d2a27183974cf4987b7129abbc02d97e3550c15766d5ce5b1221`.
Полные flags/toolchain/source/generated checks — candidate BUILD-MANIFEST.json.
Состав: собственные UART1/2, static RT tasks, bounded rings, events/progress counters,
optional observer PAGE-only, main-TWDT handoff, guarded OTA validation (disabled by default).
Это не полнофункциональная legacy/Web/MQTT firmware и не офисный OTA-ready образ.
Публичный bench-профиль без SSID, с USB provisioning; secrets.example не защищает deployment.

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



## Candidate 2026.10.12.5

| Variant | FW | OTA | Factory | Status |
|---|---|---|---|---|
| office-passive | 2026.10.12.5 | 1066816 bytes, SHA256 `b92c7feb47d71f0af486df62e5f20d70efd7ada65a1636fa25629970b422f1f7` | 1132352 bytes, SHA256 `ac373c4935e985a5a0d12e3e55e9ea105808945d280e1d8eb8088ddafcb17ba3` | Local build PASS; ESPHome 2026.9.0 / IDF 5.5.5; binaries contain office secrets and are kept outside public GitHub; PASSIVE controls/active GET disabled; hardware NOT_TESTED; NOT_DEPLOYED; OFFICE_READY=false |
