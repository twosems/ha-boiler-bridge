# Артефакты .6

Реальная сборка ESPHome 2026.9.0 успешна. Хэши factory/OTA и исходников: BUILD-MANIFEST.json. Бинарники с секретами не хранятся в публичном Git. Аппаратные испытания НЕ выполнены; office_ready=false.

При сборке использован локальный POSIX getppid workaround в idf_component_manager/cmake_pid.py из-за недоступности PID через /proc. ASan/UBSan включены; LeakSanitizer отключён из-за ограничения /proc.
