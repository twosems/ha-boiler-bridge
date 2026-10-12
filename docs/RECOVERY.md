# Как восстановить работу после потери диалога

## Быстрое продолжение
Передать новой ветке ссылку на этот файл и сказать:

> Продолжаем «ХА — котёл». Прочитай AGENTS.md, docs/CONTEXT.md, docs/VERSIONS.md, docs/DECISIONS.md, docs/DEVELOPMENT.md и последнюю запись WORKLOG. Сначала read-only сверка Git, фоновых сборок, целевого ESP и сети. Не прошивай ESP и не включай механизмы. Учти два разных .3, инцидент домашнего Wi-Fi в .1 и расхождение версии Boiler Bridge. Продолжай с next step, не повторяй согласованный UI-аудит.

## Источники истины
1. Физический экран/проводка + проверка устройства с временем.
2. Git commit + manifest + SHA-256 фактического файла + полный журнал сборки.
3. Документы этого репозитория и latest WORKLOG.
4. Историческая переписка: содержит контекст и решения, но может описывать отменённые варианты и недоказанные статусы.

Каждое утверждение помечать: USER_REPORT, HISTORY, FILE_VERIFIED, BUILD_PASS, DEVICE_VERIFIED. Если текущая версия неизвестна — так и записать. Не искать неизвестный архив под выдуманным именем и не считать каталог stable доказательством стабильности.

## Точные оригиналы
`recovery/boiler-originals-20261012.tar.xz.encrypted` содержит исходники, оригинальные secrets.yaml, OTA/factory для восьми снимков, DE-зеркала карточек и оригинальную переписку. Изображений в предоставленном Markdown нет. Открытые исходники в `firmware/snapshots` используются с локальным `secrets.yaml` из шаблона; они не публикуют пароль.

Формат: JSON-header + LF + AES-256-GCM ciphertext; AES key завернут RSA-OAEP-SHA256. Закрытый ключ `boiler-recovery-private-20261012.pem` находится в отдельном файле **boiler-recovery-key-20261012.zip**, переданном пользователю, и не в GitHub. Для восстановления:

```sh
python -m pip install cryptography
python tools/decrypt_backup.py recovery/boiler-originals-20261012.tar.xz.encrypted /path/to/boiler-recovery-private-20261012.pem /path/to/originals.tar.xz
```

Скрипт проверяет hash encrypted archive и AES-GCM до записи. Распаковывать только в приватную новую директорию; внутри есть сетевые секреты. Сверить каждый файл с `original-files.json`. Не устанавливать .1 автоматически после распаковки: домашний SSID был причиной потери офиса.

## Перед первой новой сборкой
Проверить цель/SSID/board/UART GPIO/toolchain/partitions; установить локальные секреты, не выводить их в чат. Название .3 не выбирает вариант: использовать snapshot_id. Не собирать firmware в каталог старого снимка — скопировать в отдельную рабочую директорию и назначить новый номер после изменения.

## Шаблон контрольной записи
Дата и зона времени; цель и последние команды пользователя; repo/branch/base/head SHA; host id/name; target address/network; installed firmware (или unknown); candidate/version/variant/hash; выполненные проверки и evidence; фоновые job IDs; активные запреты; rollback; следующий один конкретный шаг.
