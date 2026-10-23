# Обновление 2026-10-12: FW 2026.10.12.6

Полный кандидат собран: изолированный UART мост, защищённые активные запросы/кнопки, телеметрия, Web/MQTT/HA, журнал смены Wi-Fi, ручной Setup. Проверки host transport/network/web прошли; аппаратура не проверена, ACTIVE_REQUESTS=OFF. Текущая установленная версия неизвестна после неудачной OTA. Следующий этап: USB backup-first и проверка в офисе 13 октября. Контракт/ограничения/артефакты: firmware/candidates/2026.10.12.6/. Секреты не публикуются.

## 2026-10-12 около 20:xx Tomsk — офисный `.5` кандидат

Создана ветка `feature/boiler-fw-2026-10-12-5`. Взята актуальная сеть `2Soffice`
из скриншота; секрет только в игнорируемом локальном `secrets.yaml`. `.5` объединяет
TransportV4 и прежний parser только как observer mirror consumer. Из старого proxy
удалены UART read/write task и fallback; loop обрабатывает только зеркало. Команды
котлу и активные GET отключены, веб-интерфейс явно помечен PASSIVE.
`esphome config`, py_compile и node --check PASS. Первый compile запуск превысил
90s tool limit и был остановлен; второй job `job_7a53a89ba4e6` превысил лимит 600s. Повторный job `job_ad51fd2eb7de`
запущен на DE; во время длительного C++ этапа SentinelX agent DE перешёл offline.
Job пока числится running, его результат и наличие бинарника не подтверждены. .1 не перепрошивалась; ни офисный ESP, HA, ни MQTT не
изменялись. Никаких hardware/serial-peer тестов не было. Это не full-function office
replacement и не разрешённый к установке образ, пока компиляция и gates не закрыты.

# Журнал контрольных точек

## 2026-10-12 около 20:02 Tomsk — запрос офиса FW .5

Пользователь передал SSID `2Soffice` (скрин) и пароль Wi-Fi; секрет не заносится
в документацию/Git. На office-pc повторно прочитаны ESP `.1`, IP `.172`, SSID
`2Soffice`, MQTT `OFF`, broker `.203`, login `Sergey`. HA/MQTT endpoints были
доступны с ПК; неполадка связи ESP↔broker не установлена.

Ставить `.4` нельзя: она PAGE_ONLY без прежнего HA/MQTT/управления. `.2` сохраняет
старую функциональность, но не устраняет audit P0 для watchdog/recovery. Для офисного
`.5` предстоит связать parser-copy с единственным UART transport, затем добавить
проверенное владение командами; второй UART writer/legacy fallback запрещены.
Пользовательская команда «сделай» относится к подготовке такой замены. На этой
контрольной точке сборка `.5` и прошивка не выполнены; live ESP не менялся.

## 2026-10-12 19:44 Tomsk — office-pc enrolled and checked

По прямой команде пользователя проверен DESKTOP-H0NBTQ5 / office-pc /
host_6d74202de72b5430, SentinelX0.23.17 operational. IP192.168.1.144.
ESP FW.1 и WiFi2Soffice подтверждены live GET; HA8123 и MQTT1883 доступны.
MQTT OFF повторно, broker/login сохранены правильно; причина требует диагностики.
Uptime8344s, BTEMP40.2 не считать свежей температурой без UART/physical evidence.
SerialPort inventory пуст. Работа только read-only, без restart/config/firmware changes.

## 2026-10-12 около 17:20 Tomsk — MQTT восстановлен

Пользователь дал актуальные office credentials и уточнил регистр login.
Проверка самого брокера: CONNACK0. Все native text fields выставлены и сверены;
пароль не записан в документы/Git. Broker192.168.1.203:1883, final MQTT ON.
Значение пароля промежуточно оказалось в broker field (причина не установлена),
поэтому проверка остановила graceful restart ДО POST. Затем все поля исправлены;
MQTT подключился без выполненного нами restart. Наблюдалось снижение uptime
>1600s→152s, origin unknown; последний uptime182s. BTEMP reported40.2, без физической сверки.
Firmware .1, .4 NOT DEPLOYED. Новый стабильный офисный replacement не готов;
UI/architecture work остаётся открытым, подключение MQTT не заменяет reflash.

## 2026-10-12 около 17:14 Tomsk — живой офисный ESP, MQTT recovery

Подтверждены FW2026.10.12.1 / WiFi2Soffice / 192.168.1.172 по live GET через
SentinelX host_637532310b5cb99d. Пользователь подтвердил исправный физический экран.
Live JS показывает rerender формы на SSE state; saved MQTT broker содержал мусор.
Исправлен только broker host на офисный 192.168.1.203: text/set HTTP200 + GET read-back,
нажат штатный MQTT apply endpoint, не кнопка котла. Пароли не менялись.
MQTT CONNECT возвращает CONNACK5 с текущими и одними ранее известными credentials;
публикаций/подписок диагностическим клиентом не было. MQTT OFF, нужен офисный аккаунт.
ESP не перезагружался, OTA не запускалась. .4 bench не выдаётся за stable office
replacement; пользовательский запрос срочной перепрошивки остаётся невыполненным.

## 2026-10-12 — .4 окончательная сборка и Git-артефакты

Source SHA f801357f21ef7aa6aaa813096da99b903ed606a4, отдельная feature-ветка.
DE job_8a7cc4fe3ffb succeeded exit0, завершение 08:04:05 UTC / 15:04:05 Tomsk.
OTA 912384 bytes, SHA256 0f8902261d0098a6bc53aecd1a261225c09a5082b10b065644f76b5673cf6e70;
factory 977920 bytes, SHA256 49b332333e49d2a27183974cf4987b7129abbc02d97e3550c15766d5ce5b1221.
RAM73568/180736; flash912263/1835008. ESPHome2026.9.0 / IDF5.5.5.
Source→generated C++/H equality, both wrapper symbols/link flags, explicit rollback,
USE_OTA_ROLLBACK, watchdogs, static component storage and no standard safe-mode branch PASS.
100000 ring operations, 100000 mirror records, 100000 health snapshot writes,
numeric framing/future-timestamp/rollover/boot gates PASS under ASan/UBSan;
17 structural checks PASS. LeakSanitizer disabled due ptrace, no claim of leak-test PASS.
Archive b/c pre-final builds not published as firmware artifacts. Final factory/OTA,
manifest, final log and commissioning checklist preserved in feature branch only.
Public bench placeholders only; no private user credentials. Not office-ready,
not hardware-tested, not deployed. No ESP/HA restart or actuator operation.
Next step: USB/target clarification, verified backup/recovery and passive serial-peer bench.

## 2026-10-12 — реализация .4, промежуточная точка

Пользователь разрешил разработку, попросил ускорить и подготовить прошивку.
Base cc642c0; feature/boiler-fw-2026-10-12-4. Созданы отдельный transport component,
contract, public bench config, C++ core harness и structural regression checker.
100000 randomized ByteRing operations и 100000 SPSC records PASS с ASan/UBSan;
LeakSanitizer отдельно недоступен из-за ptrace. 15 source-only assertions PASS.
Отключено неоднозначное stock GET/reply matching: только PAGE и diagnostics.
Supervisor создаётся прежде bridge; main TWDT удаляется только после готовности.
Проверен установленный ESPHome 2026.9.0 HAL: arch_feed_wdt безусловный;
добавлен wrapper только для снятого loopTask, real reset сохранён остальным tasks.
Первый build job_5c52ebb18a0a FAILED_CONFIG: all-zero API key rejected;
заменён публичным nonzero bench key. DE job_9df373508348 компилирует промежуточный b.
После последней watchdog-правки нужен финальный rebuild; успешность НЕ заявлена.
Host host_b5648365d9b64ade; build dir /var/lib/sentinelx/uploads/boiler-fw-20261012-4;
лог b: /var/lib/sentinelx/uploads/boiler-fw-20261012-4-build-b.log.
ESP, HA, GPIO исполнительных механизмов, сеть офиса не изменены.
Next: финальная сборка → actual hashes/generated checks → USB/стенд/recovery gate.

Дополнение: промежуточные b/c BUILD_PASS; compiled .4 ещё не frozen binary.
Исправлена межъядерная гонка age: timestamp может быть новее sampled now;
unsigned subtraction не должен давать ложный fatal stall. Tests PASS.
Generated sdkconfig показал rollback disabled: ESPHome final_validate выключает
standard rollback без safe_mode. Компонент включает IDF option/define напрямую;
16 structural checks PASS. Финальный build d: job_e7d85d58cc0f на том же DE;
лог /var/lib/sentinelx/uploads/boiler-fw-20261012-4-build-d.log. Ждём actual результат.

Последняя правка: atomic health revision защищает supervisor snapshot от сочетания
старого pending и нового cleared timestamp. Core concurrency test добавлен;
17 structural checks PASS. Финальный incremental job_8a7cc4fe3ffb запускается после d;
лог /var/lib/sentinelx/uploads/boiler-fw-20261012-4-build-e.log. Source frozen далее;
аппаратные оговорки не снимаются успешной компиляцией.

## 2026-10-12 — перенос ветки в GitHub
Запрос: перечитать историю 2–3 раза, сохранить текущие firmware/версии и рекомендации для восстановления диалога.
История изучена по хронологии, затем отдельно по артефактам/сетям и по финальным решениям/открытым задачам. Переданная копия: 3645 строк; изображения не вложены.
Собраны восемь source snapshots, 16 binary originals, original-file SHA manifest; сверено совпадение .1 с историей.
Сохранены .2 и две .3 без установки и без переименования их встроенных версий.
Original backups зашифрованы AES-GCM/RSA-OAEP; private key не передавался SentinelX/GitHub. В архив добавлена точная исходная переписка локально. Все 82 файла original manifest проверены после расшифрования до упаковки.
Сохранены отдельные DE-зеркала карточек/const.py с точным происхождением. HACS-код, работающие HA и ESP не изменялись.
Найдены: успешные журналы .2/rtbridge; ранний failed job isolated-transport + имеющийся бинарник; расхождение GitHub integration 0.2.5 с историей 0.3.1; отсутствие текущего подтверждения устройства.
Next step: проверить соответствие исходник↔бинарник двух .3, выбрать кандидат, выполнить пассивный стенд. OTA/изменение параметров не разрешены этим архивированием.

Дополнительная сверка: найден BUILD-2026.10.12.3.txt isolated-transport; его binary hashes совпадают. Generated C++/H .2 и обоих .3 побайтно совпадают с source snapshots. Manifest isolated-transport подтверждает NOT FLASHED и домашний Wi-Fi как deployment blocker. Это не стендовый PASS.

## 2026-10-12 — повторный аудит правильности архитектуры
Запрос пользователя: заново проверить выводы и организацию firmware под проект, исправить рекомендации, обосновать необходимые доработки.
Baseline Git: 878f007d78ff2c01e6b62793e10e556b8644c7f1. Проверены оба .3 и фактические generated ESPHome HAL/safe_mode/UART, sdkconfig/partitions/main.cpp на DE; сопоставлены первичные источники ESP-IDF 5.5 и ESPHome.
Подтверждено: глобальный main-loop TWDT остаётся подписан с panic5s; safe_mode отключает bridge component; пассивный rtbridge fallback снова включает команды/старый parser-first путь; isolated-transport требует observer allocation для запуска; counters не измеряют IDF FIFO errors; heartbeat не равен TX progress. Standard OTA подтверждается по времени/shutdown, не bridge-health; compiled bootloader rollback есть, installed support неизвестен.
Исправлены рекомендации по software/IDF TX buffers, реальной core isolation, soft-control vs fatal-transport watchdog, recovery/rollback, flash pauses и command/response ownership.
Offline checks: 8 групп проверок извлечённых C++ pure functions (ring/Nextion numeric), 3 structural assertions, 2 illustrative failure models; PASS. Это не FreeRTOS/hardware PASS и не диагноз причины прошлой остановки котла.
Результат: docs/ARCHITECTURE-AUDIT-20261012.md; обновлены AGENTS/CONTEXT/DEVELOPMENT. Firmware snapshots, бинарники, HA, ESP, котельные настройки не изменены.
Next step: реализация нового пассивного .4 по скорректированному контракту и стенд, затем отдельный этап transaction-safe управления. Старые .3 остаются архивом, deployment blocked.


## 2026-10-12 около 21:09 Tomsk — .5 собрана локально

По прямой просьбе пользователя сборка выполнена в текущем изолированном workspace; office-pc и DE не использовались. ESPHome 2026.9.0 / ESP-IDF 5.5.5. Конфигурация прошла `esphome config`; финальная правка отключила стандартный `safe_mode`, который мог пропустить запуск UART-моста, и исправила диагностическую метку версии с .4 на .5.

BUILD_PASS: OTA 1066816 bytes / SHA256 `b92c7feb47d71f0af486df62e5f20d70efd7ada65a1636fa25629970b422f1f7`; factory 1132352 bytes / SHA256 `ac373c4935e985a5a0d12e3e55e9ea105808945d280e1d8eb8088ddafcb17ba3`. RAM92360/180736, flash1066695/1835008. Generated C++/H match source, `should_enter_safe_mode` early-return отсутствует, rollback=`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`, оба linker wrappers есть в ELF. Portable ASan/UBSan core tests PASS (100000 ring ops и mirror records).

Артефакты находятся в `/workspace/scratch/f608745efdda/boiler-firmware-output/`; OTA/factory binary содержат офисные секреты и не добавляются в публичный GitHub. Ветка GitHub получает исправленные исходники, manifest, хэши и статус. `.5` остаётся PASSIVE: controls и active GET выключены; hardware/bench, MQTT preference migration и sensor freshness не проверены; `OFFICE_READY=false`, NOT_DEPLOYED. Живой ESP не затрагивался.

## 2026-10-12 — попытка OTA .5, результат не подтверждён

По прямой команде пользователя подготовлен живой OTA. На `office-pc` размещены
оба приватных образа и проверены точные SHA: `.5` OTA 1066816 bytes /
`b92c7feb47d71f0af486df62e5f20d70efd7ada65a1636fa25629970b422f1f7`; исходный
`.1` recovery OTA 1076832 bytes /
`e99f27b74c0c9d05246dc97604359034bfabd70fb9eade909c5b1e39bdb1a0c3`.
На office-pc нет COM-портов. Перед POST `/update` устройство отвечало HTTP 200;
первая попытка TCP timed out до соединения. Вторая дошла до ESP и получила HTTP
408. После неё `.172` перестал отвечать на 80, 3232 и 6053; сканер пользователя
по-прежнему показывает `boiler-proxy`, `.172`, MAC `A0:A3:B3:2A:87:F8`.
Видимая сеть `Boiler-Setup` не обнаружена. Версия установленной прошивки
UNKNOWN — нельзя сказать, записался ли `.5`; повторный OTA не отправлять.
Recovery `.1` исторически содержит домашний Wi-Fi профиль, а rollback support
в bootloader живого устройства не подтверждён. Требуется сначала восстановить
доступ и определить фактическую версию; затем выбрать проверенный recovery.


## 2026-10-20 — синхронизация истории до.16 вGitHub

По прямой просьбеСергея все доступные исходники/изменения собраны в feature/firmware-history-through-16-20261020 поверх e3e4ccd (.6), который содержит историюmain/.4/.5/.6. Восстановлены.7/.8/.9/.10/.12 и перенесены.13/.14/.15/.16, тесты, отчёты и история локальных коммитов (Gitbundle). .4-office сохранён отдельным recovered snapshot. Оригиналы десяти версий и отдельный пакет.6 зашифрованы для существующего recoverypublickey. Не найден отдельный.11; оригиналы.5 не найдены, известные hashes/source сохранены. Никаких новых OTA/restart/нажатий не выполнялось.

Источник установки.16: DEbuildjob_7239fd426378,OTAjob_746a6b150032; отчётDEPLOYMENT-20261020-16.md. USER_REPORT17:10 физическая сверкапереходов/значений. Новых liveзамеров после этого не было. Nextstep: настоящийACTIVE_REQUESTS вHACS-карточке после определенияruntimeJS иMQTTмаршрута дома.

## 2026-10-23 — исследование доступа к Nextion без переключения экрана

Пользователь запросил доработать тест, оценить удалённое OTA и запросить разрешение до установки. Создан кандидат 2026.10.23.17 как копия .16 в отдельном каталоге, с одиночным, явно запускаемым OFFSCREEN_READ только для page10.TsR.val; интегрирован отказ на странице 10 и при ACTIVE_REQUESTS, независимое диагностическое состояние, host-тесты ASan/UBSan PASS. Базовая конфигурация ESPHome PASS, новая сборка выполняется отдельно. Офисный ПК видит ESP TCP 80/3232/6053, HTTP 401 (нужна авторизация). Отправка с DE по HTTPS git push невозможна без учётных данных; опубликовано через GitHub connector в отдельном PR #2. Ни OTA, ни restart, ни фактические команды ESP/котлу не выполнялись. Документ: OFFSCREEN-PROBE-20261023.md. Фоновая запись значений пока не реализована: для этого неизвестен протокол сохранения параметров контроллером MINI.
