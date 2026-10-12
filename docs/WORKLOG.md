# Журнал контрольных точек

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
