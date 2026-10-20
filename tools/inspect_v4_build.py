#!/usr/bin/env python3
"""Verify a finished ESPHome .4 build and emit a JSON evidence manifest to stdout.

No flashing or host configuration changes. Usage:
  python tools/inspect_v4_build.py SOURCE_ROOT BUILD_LOG SOURCE_COMMIT JOB_ID
"""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

source, log_path, commit, job = Path(sys.argv[1]), Path(sys.argv[2]), sys.argv[3], sys.argv[4]
build = source / '.esphome/build/boiler-proxy'
log = log_path.read_text()
assert 'INFO Successfully compiled program.' in log, 'final build did not succeed'
sdk = (build / 'sdkconfig.boiler-proxy').read_text()
defines = (build / 'src/esphome/core/defines.h').read_text()
main = (build / 'src/main.cpp').read_text()
main_code = '\n'.join(x for x in main.splitlines() if not x.lstrip().startswith('//'))
assert 'CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y' in sdk
assert '#define USE_OTA_ROLLBACK' in defines
assert 'CONFIG_ESP_TASK_WDT_PANIC=y' in sdk
assert 'CONFIG_ESP_TASK_WDT_TIMEOUT_S=5' in sdk
assert 'CONFIG_ESP_INT_WDT=y' in sdk
assert 'CONFIG_FREERTOS_HZ=1000' in sdk
assert 'should_enter_safe_mode(' not in main_code
assert 'transport->set_allow_boot_confirm(false);' in main_code
assert 'transport->set_observer_enabled(true);' in main_code
assert 'set_reboot_timeout(0)' in main_code
assert 'wifi_wificomponent_id->set_ssid(' not in main_code
assert 'static unsigned char boiler_transport_v4__transport__pstorage' in main_code

def hash_file(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

source_hashes = {}
for name in ['boiler-proxy.yaml', 'secrets.example.yaml']:
    source_hashes[name] = hash_file(source / name)
component = source / 'components/boiler_transport_v4'
for name in ['__init__.py', 'bridge_core.h', 'boiler_transport_v4.h', 'boiler_transport_v4.cpp']:
    source_hashes['components/boiler_transport_v4/' + name] = hash_file(component / name)
    if name.endswith(('.h', '.cpp')):
        assert hash_file(component / name) == hash_file(build / 'src/esphome/components/boiler_transport_v4' / name)
assert hash_file(source / 'secrets.yaml') == hash_file(source / 'secrets.example.yaml'), 'not public bench profile'

nm = Path('/opt/sentinelx-cloud-core/.cache/esphome/idf/tools/xtensa-esp-elf/esp-14.2.0_20260121/xtensa-esp-elf/bin/xtensa-esp32-elf-nm')
symbols = subprocess.run([str(nm), str(build / 'build/firmware.elf')], check=True, capture_output=True, text=True).stdout
required_symbols = ['__wrap_esp_task_wdt_reset', '__wrap_esp_ota_mark_app_valid_cancel_rollback',
                    'esp_task_wdt_reset', 'esp_ota_mark_app_valid_cancel_rollback']
for symbol in required_symbols:
    assert any(x.split()[-1] == symbol for x in symbols.splitlines() if x.split()), symbol
ninja = (build / 'build/build.ninja').read_text()
for flag in ['--wrap=esp_task_wdt_reset', '--wrap=esp_ota_mark_app_valid_cancel_rollback']:
    assert flag in ninja, flag

artifacts = {}
for name in ['firmware.ota.bin', 'firmware.factory.bin', 'firmware.elf', 'bootloader/bootloader.bin']:
    p = build / 'build' / name
    artifacts[name] = {'bytes': p.stat().st_size, 'sha256': hash_file(p)}
for p in [build/'partitions.csv', build/'partition_table/partition-table.bin']:
    if p.exists():
        artifacts[str(p.relative_to(build))] = {'bytes': p.stat().st_size, 'sha256': hash_file(p)}
memory = [re.sub(r'\x1b\[[0-9;]*[A-Za-z]', '', x) for x in log.splitlines() if x.startswith(('RAM:', 'Flash:', 'INFO Build Info:'))]
manifest = {
    'version': '2026.10.12.4', 'variant': 'passive-page-only-public-bench',
    'repository': 'twosems/ha-boiler-bridge', 'branch': 'feature/boiler-fw-2026-10-12-4',
    'source_commit': commit, 'host_id': 'host_b5648365d9b64ade', 'job_id': job,
    'build_status': 'PASS', 'generated_checks': 'PASS', 'hardware_status': 'NOT_TESTED',
    'deployment_status': 'NOT_DEPLOYED', 'office_ready': False,
    'profile': 'PUBLIC_BENCH_PLACEHOLDERS_NO_SSID_USB_PROVISIONING',
    'write_capable': False, 'boot_confirmation_enabled': False,
    'board': 'esp32dev / classic dual-core ESP32', 'flash_bytes': 4194304,
    'uart': [{'port':1,'rx':16,'tx':17},{'port':2,'rx':18,'tx':19}], 'serial': '9600 8N1',
    'toolchain': {'esphome':'2026.9.0','esp_idf':'5.5.5','gcc':'14.2.0_20260121','configured_framework':'arduino'},
    'build_root': str(build), 'log_path': str(log_path), 'log_sha256': hash_file(log_path),
    'memory_and_config': memory, 'artifacts': artifacts, 'source_hashes': source_hashes,
    'checks': ['explicit_IDF_rollback', 'standard_safe_mode_not_registered', 'public_bench_profile',
               'static_component_storage', 'source_equals_generated_CPP', 'watchdogs_retained',
               'both_linker_wrappers_present', 'boot_confirmation_disabled', 'wifi_api_no_restart'],
    'limits': ['bench timeouts not validated on boiler', 'reset/OTA/power failure interrupt UART',
               'installed bootloader and previous image unknown', 'no guaranteed Wi-Fi NVS migration',
               'no MQTT/actuators/setpoint attribution', 'no hardware/runtime fault-injection pass'],
}
print(json.dumps(manifest, ensure_ascii=False, indent=2))
