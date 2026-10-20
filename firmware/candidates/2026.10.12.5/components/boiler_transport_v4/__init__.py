import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, text_sensor
from esphome.components.esp32 import add_idf_sdkconfig_option
from esphome.const import CONF_ID

DEPENDENCIES = ['esp32']
AUTO_LOAD = ['sensor', 'text_sensor']
ns = cg.esphome_ns.namespace('boiler_transport_v4')
BoilerTransportV4 = ns.class_('BoilerTransportV4', cg.Component)
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(BoilerTransportV4),
    cv.Optional('observer_enabled', default=True): cv.boolean,
    cv.Optional('external_mirror_consumer', default=False): cv.boolean,
    cv.Optional('allow_boot_confirm', default=False): cv.boolean,
    cv.Optional('diagnostics'): text_sensor.text_sensor_schema(),
    cv.Optional('current_page'): sensor.sensor_schema(accuracy_decimals=0),
    cv.Optional('boiler_setpoint'): sensor.sensor_schema(unit_of_measurement='°C', accuracy_decimals=1),
    cv.Optional('dhw_setpoint'): sensor.sensor_schema(unit_of_measurement='°C', accuracy_decimals=1),
    cv.Optional('mode_raw'): text_sensor.text_sensor_schema(),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    # ESPHome disables its rollback setting when standard safe_mode is disabled.
    # This component owns commissioning, so enable IDF rollback explicitly.
    add_idf_sdkconfig_option('CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE', True)
    cg.add_define('USE_OTA_ROLLBACK')
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_observer_enabled(config['observer_enabled']))
    cg.add(var.set_external_mirror_consumer(config['external_mirror_consumer']))
    cg.add(var.set_allow_boot_confirm(config['allow_boot_confirm']))
    for key in ('current_page', 'boiler_setpoint', 'dhw_setpoint'):
        if key in config:
            s = await sensor.new_sensor(config[key])
            cg.add(getattr(var, 'set_'+key)(s))
    for key in ('diagnostics', 'mode_raw'):
        if key in config:
            s = await text_sensor.new_text_sensor(config[key])
            cg.add(getattr(var, 'set_'+key)(s))
    # No caller outside the bridge-health coordinator may confirm OTA.
    cg.add_build_flag('-Wl,--wrap=esp_ota_mark_app_valid_cancel_rollback')
    cg.add_build_flag('-Wl,--wrap=esp_task_wdt_reset')
