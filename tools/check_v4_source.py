#!/usr/bin/env python3
"""Structural regression checks, not a substitute for compiled/hardware tests."""
from pathlib import Path
import json

root = Path(__file__).resolve().parents[1]
candidate = root / 'firmware/candidates/2026.10.12.4'
source = (candidate / 'components/boiler_transport_v4/boiler_transport_v4.cpp').read_text()
schema = (candidate / 'components/boiler_transport_v4/__init__.py').read_text()
config = (candidate / 'boiler-proxy.yaml').read_text()
checks = {}
def check(name, condition):
    assert condition, name
    checks[name] = 'PASS'

check('one_tx_writer_no_command_insertion', source.count('uart_tx_chars(') == 1 and
      all(x not in source for x in ['uart_write_bytes(', 'request_safe_click(', 'request_refresh(', 'send_command(']))
check('zero_idf_tx_ring', 'uart_driver_install(d.port,2048,0,16,&d.events,0)' in source)
check('no_rx_flush', 'uart_flush(' not in source)
check('supervisor_created_before_bridge', source.index('supervisor_task_=xTaskCreate') < source.index('bridge_task_=xTaskCreate'))
check('bridge_ready_before_main_wdt_removal', source.index('if(!ready_.load()||!bridge_wdt_.load())') < source.index('esp_task_wdt_delete(nullptr)'))
check('wdt_feed_exemption_only_detached_loop', 'detached==xTaskGetCurrentTaskHandle()' in source and
      'return __real_esp_task_wdt_reset();' in source and '--wrap=esp_task_wdt_reset' in schema)
check('silence_not_a_transport_fault', 'tx_stalled(d.pending.load()' in source and 'if(!ready_.load())' in source)
check('copy_loss_checked_before_decoding', source.index('if(drops!=seen_drops_)') < source.index('i<16&&mirror_.pop(b)'))
check('uart_epoch_invalidates_without_new_copy', 'if(epoch!=seen_epoch_){invalidate_observer();seen_epoch_=epoch;}' in source)
check('stock_get_reply_attribution_disabled', 'stock_request_' not in source and 'request_pending_' not in source)
check('standard_safe_mode_disabled', 'safe_mode:\n  disabled: true' in config)
check('custom_idf_rollback_explicit', "add_idf_sdkconfig_option('CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE', True)" in schema and "cg.add_define('USE_OTA_ROLLBACK')" in schema)
check('wifi_api_loss_no_restart', config.count('reboot_timeout: 0s') == 2)
check('public_bench_profile_no_ssid', '\n  ssid:' not in config and '\n  password:' not in config)
check('no_actuators_mqtt_http_ota', all(x not in config for x in ['\nbutton:', '\nswitch:', '\nnumber:', '\nmqtt:', 'platform: web_server']))
check('no_auto_confirm_for_bench', 'allow_boot_confirm: false' in config and
      'if(allow_boot_confirm_ && !confirmed' in source and '--wrap=esp_ota_mark_app_valid_cancel_rollback' in schema)
print(json.dumps({'checks': checks, 'scope': 'static source only; no driver, task, GPIO or hardware execution'}, indent=2))
