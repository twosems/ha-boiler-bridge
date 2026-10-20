import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, text_sensor, button
from esphome.const import CONF_ID, DEVICE_CLASS_TEMPERATURE, UNIT_CELSIUS, UNIT_AMPERE, STATE_CLASS_MEASUREMENT

DEPENDENCIES = ["esp32"]
AUTO_LOAD = ["sensor", "text_sensor", "button"]

boiler_proxy_ns = cg.esphome_ns.namespace("boiler_proxy")
BoilerProxy = boiler_proxy_ns.class_("BoilerProxy", cg.Component)
BoilerProxyButton = boiler_proxy_ns.class_("BoilerProxyButton", button.Button)
BoilerProxyClickButton = boiler_proxy_ns.class_("BoilerProxyClickButton", button.Button)

CONF_TRANSPORT_ID = "transport_id"
Transport = cg.esphome_ns.namespace("boiler_transport_v4").class_("BoilerTransportV4", cg.Component)
CONF_CURRENT_PAGE = "current_page"
CONF_CURRENT_SCREEN = "current_screen"
CONF_MODE_RAW = "mode_raw"
CONF_MODE_LABEL = "mode_label"
CONF_DHW_TOP_BUTTON_TEXT = "dhw_top_button_text"
CONF_ROOM_THERMOSTAT_BUTTON_TEXT = "room_thermostat_button_text"
CONF_IGNITION_FAN_BUTTON_TEXT = "ignition_fan_button_text"
CONF_FEED_BUTTON_TEXT = "feed_button_text"
CONF_REVERSE_BUTTON_TEXT = "reverse_button_text"
CONF_SCREENSAVER_BUTTON_TEXT = "screensaver_button_text"

CONF_PASSWORD_KEYMAP = "password_keymap"
CONF_BOILER_SETPOINT = "boiler_setpoint"
CONF_DHW_SETPOINT = "dhw_setpoint"
CONF_BOILER_TEMPERATURE = "boiler_temperature"
CONF_DHW_TEMPERATURE = "dhw_temperature"
CONF_REVERSE_TRIP_CURRENT = "reverse_trip_current"
CONF_MOTOR_CURRENT = "motor_current"
CONF_PID_KP = "pid_kp"
CONF_PID_KI = "pid_ki"
CONF_PID_KD = "pid_kd"
CONF_AUTO_FAN_CORRECTION = "auto_fan_correction"
CONF_AUTO_FAN_RANGE_MIN = "auto_fan_range_min"
CONF_AUTO_FAN_RANGE_MAX = "auto_fan_range_max"
CONF_AUTO_FEED_CORRECTION = "auto_feed_correction"
CONF_AUTO_FEED_RANGE_MIN = "auto_feed_range_min"
CONF_AUTO_FEED_RANGE_MAX = "auto_feed_range_max"
CONF_AUTO_PAUSE = "auto_pause"
CONF_SUPPORT_BLOW = "support_blow"
CONF_SUPPORT_FEED = "support_feed"
CONF_SUPPORT_PAUSE = "support_pause"
CONF_IGNITION_FAN = "ignition_fan"
CONF_IGNITION_FAN_BUTTON_STATE = "ignition_fan_button_state"
CONF_FEED_BUTTON_STATE = "feed_button_state"
CONF_REVERSE_BUTTON_STATE = "reverse_button_state"
CONF_MODE_AUTO_BUTTON_STATE = "mode_auto_button_state"
CONF_MODE_IGNITION_BUTTON_STATE = "mode_ignition_button_state"
CONF_MODE_SUPPORT_BUTTON_STATE = "mode_support_button_state"
CONF_DHW_TOP_BUTTON_STATE = "dhw_top_button_state"
CONF_ROOM_THERMOSTAT_BUTTON_STATE = "room_thermostat_button_state"
CONF_SCREENSAVER_BUTTON_STATE = "screensaver_button_state"
CONF_REFRESH = "refresh"
CONF_CONTROLS = "controls"
CONF_PAGE = "page"
CONF_COMPONENT_ID = "component_id"

TEMP_SCHEMA = sensor.sensor_schema(unit_of_measurement=UNIT_CELSIUS, accuracy_decimals=1, device_class=DEVICE_CLASS_TEMPERATURE, state_class=STATE_CLASS_MEASUREMENT)
AMP_SCHEMA = sensor.sensor_schema(unit_of_measurement=UNIT_AMPERE, accuracy_decimals=1, state_class=STATE_CLASS_MEASUREMENT)
CONTROL_SCHEMA = button.button_schema(BoilerProxyClickButton).extend({
    cv.Required(CONF_PAGE): cv.int_range(min=0, max=255),
    cv.Required(CONF_COMPONENT_ID): cv.int_range(min=1, max=255),
})

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(BoilerProxy),
    cv.Required(CONF_TRANSPORT_ID): cv.use_id(Transport),
    cv.Optional(CONF_CURRENT_PAGE): sensor.sensor_schema(accuracy_decimals=0),
    cv.Optional(CONF_CURRENT_SCREEN): text_sensor.text_sensor_schema(),
    cv.Optional(CONF_MODE_RAW): text_sensor.text_sensor_schema(),
    cv.Optional(CONF_MODE_LABEL): text_sensor.text_sensor_schema(),
    cv.Optional(CONF_DHW_TOP_BUTTON_TEXT): text_sensor.text_sensor_schema(),
    cv.Optional(CONF_ROOM_THERMOSTAT_BUTTON_TEXT): text_sensor.text_sensor_schema(),
    cv.Optional(CONF_IGNITION_FAN_BUTTON_TEXT): text_sensor.text_sensor_schema(),
    cv.Optional(CONF_FEED_BUTTON_TEXT): text_sensor.text_sensor_schema(),
    cv.Optional(CONF_REVERSE_BUTTON_TEXT): text_sensor.text_sensor_schema(),
    cv.Optional(CONF_SCREENSAVER_BUTTON_TEXT): text_sensor.text_sensor_schema(),

    cv.Optional(CONF_PASSWORD_KEYMAP): text_sensor.text_sensor_schema(),
    cv.Optional(CONF_BOILER_SETPOINT): TEMP_SCHEMA,
    cv.Optional(CONF_DHW_SETPOINT): TEMP_SCHEMA,
    cv.Optional(CONF_BOILER_TEMPERATURE): TEMP_SCHEMA,
    cv.Optional(CONF_DHW_TEMPERATURE): TEMP_SCHEMA,
    cv.Optional(CONF_REVERSE_TRIP_CURRENT): AMP_SCHEMA,
    cv.Optional(CONF_MOTOR_CURRENT): AMP_SCHEMA,
    cv.Optional(CONF_PID_KP): sensor.sensor_schema(accuracy_decimals=0),
    cv.Optional(CONF_PID_KI): sensor.sensor_schema(accuracy_decimals=0),
    cv.Optional(CONF_PID_KD): sensor.sensor_schema(accuracy_decimals=0),
    cv.Optional(CONF_AUTO_FAN_CORRECTION): sensor.sensor_schema(unit_of_measurement="%", accuracy_decimals=0),
    cv.Optional(CONF_AUTO_FAN_RANGE_MIN): sensor.sensor_schema(unit_of_measurement="%", accuracy_decimals=0),
    cv.Optional(CONF_AUTO_FAN_RANGE_MAX): sensor.sensor_schema(unit_of_measurement="%", accuracy_decimals=0),
    cv.Optional(CONF_AUTO_FEED_CORRECTION): sensor.sensor_schema(unit_of_measurement="%", accuracy_decimals=0),
    cv.Optional(CONF_AUTO_FEED_RANGE_MIN): sensor.sensor_schema(unit_of_measurement="s", accuracy_decimals=0),
    cv.Optional(CONF_AUTO_FEED_RANGE_MAX): sensor.sensor_schema(unit_of_measurement="s", accuracy_decimals=0),
    cv.Optional(CONF_AUTO_PAUSE): sensor.sensor_schema(unit_of_measurement="s", accuracy_decimals=0),
    cv.Optional(CONF_SUPPORT_BLOW): sensor.sensor_schema(unit_of_measurement="s", accuracy_decimals=0),
    cv.Optional(CONF_SUPPORT_FEED): sensor.sensor_schema(unit_of_measurement="s", accuracy_decimals=0),
    cv.Optional(CONF_SUPPORT_PAUSE): sensor.sensor_schema(unit_of_measurement="min", accuracy_decimals=0),
    cv.Optional(CONF_IGNITION_FAN): sensor.sensor_schema(unit_of_measurement="%", accuracy_decimals=0),
    cv.Optional(CONF_IGNITION_FAN_BUTTON_STATE): sensor.sensor_schema(accuracy_decimals=0),
    cv.Optional(CONF_FEED_BUTTON_STATE): sensor.sensor_schema(accuracy_decimals=0),
    cv.Optional(CONF_REVERSE_BUTTON_STATE): sensor.sensor_schema(accuracy_decimals=0),
    cv.Optional(CONF_MODE_AUTO_BUTTON_STATE): sensor.sensor_schema(accuracy_decimals=0),
    cv.Optional(CONF_MODE_IGNITION_BUTTON_STATE): sensor.sensor_schema(accuracy_decimals=0),
    cv.Optional(CONF_MODE_SUPPORT_BUTTON_STATE): sensor.sensor_schema(accuracy_decimals=0),
    cv.Optional(CONF_DHW_TOP_BUTTON_STATE): sensor.sensor_schema(accuracy_decimals=0),
    cv.Optional(CONF_ROOM_THERMOSTAT_BUTTON_STATE): sensor.sensor_schema(accuracy_decimals=0),
    cv.Optional(CONF_SCREENSAVER_BUTTON_STATE): sensor.sensor_schema(accuracy_decimals=0),
    cv.Optional(CONF_REFRESH): button.button_schema(BoilerProxyButton),
    cv.Optional(CONF_CONTROLS): cv.ensure_list(CONTROL_SCHEMA),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    transport = await cg.get_variable(config[CONF_TRANSPORT_ID])
    cg.add(var.set_transport(transport))

    sensor_setters = {
        CONF_CURRENT_PAGE: "set_current_page_sensor",
        CONF_BOILER_SETPOINT: "set_boiler_setpoint_sensor",
        CONF_DHW_SETPOINT: "set_dhw_setpoint_sensor",
        CONF_BOILER_TEMPERATURE: "set_boiler_temperature_sensor",
        CONF_DHW_TEMPERATURE: "set_dhw_temperature_sensor",
        CONF_REVERSE_TRIP_CURRENT: "set_reverse_trip_current_sensor",
        CONF_MOTOR_CURRENT: "set_motor_current_sensor",
        CONF_PID_KP: "set_pid_kp_sensor",
        CONF_PID_KI: "set_pid_ki_sensor",
        CONF_PID_KD: "set_pid_kd_sensor",
        CONF_AUTO_FAN_CORRECTION: "set_auto_fan_correction_sensor",
        CONF_AUTO_FAN_RANGE_MIN: "set_auto_fan_range_min_sensor",
        CONF_AUTO_FAN_RANGE_MAX: "set_auto_fan_range_max_sensor",
        CONF_AUTO_FEED_CORRECTION: "set_auto_feed_correction_sensor",
        CONF_AUTO_FEED_RANGE_MIN: "set_auto_feed_range_min_sensor",
        CONF_AUTO_FEED_RANGE_MAX: "set_auto_feed_range_max_sensor",
        CONF_AUTO_PAUSE: "set_auto_pause_sensor",
        CONF_SUPPORT_BLOW: "set_support_blow_sensor",
        CONF_SUPPORT_FEED: "set_support_feed_sensor",
        CONF_SUPPORT_PAUSE: "set_support_pause_sensor",
        CONF_IGNITION_FAN: "set_ignition_fan_sensor",
        CONF_IGNITION_FAN_BUTTON_STATE: "set_ignition_fan_button_state_sensor",
        CONF_FEED_BUTTON_STATE: "set_feed_button_state_sensor",
        CONF_REVERSE_BUTTON_STATE: "set_reverse_button_state_sensor",
        CONF_MODE_AUTO_BUTTON_STATE: "set_mode_auto_button_state_sensor",
        CONF_MODE_IGNITION_BUTTON_STATE: "set_mode_ignition_button_state_sensor",
        CONF_MODE_SUPPORT_BUTTON_STATE: "set_mode_support_button_state_sensor",
        CONF_DHW_TOP_BUTTON_STATE: "set_dhw_top_button_state_sensor",
        CONF_ROOM_THERMOSTAT_BUTTON_STATE: "set_room_thermostat_button_state_sensor",
        CONF_SCREENSAVER_BUTTON_STATE: "set_screensaver_button_state_sensor",
    }
    for key, setter in sensor_setters.items():
        if key in config:
            s = await sensor.new_sensor(config[key])
            cg.add(getattr(var, setter)(s))

    if CONF_CURRENT_SCREEN in config:
        ts = await text_sensor.new_text_sensor(config[CONF_CURRENT_SCREEN])
        cg.add(var.set_current_screen_sensor(ts))

    if CONF_MODE_RAW in config:
        ts = await text_sensor.new_text_sensor(config[CONF_MODE_RAW])
        cg.add(var.set_mode_raw_sensor(ts))

    text_setters = {
        CONF_MODE_LABEL: "set_mode_label_sensor",
        CONF_DHW_TOP_BUTTON_TEXT: "set_dhw_top_button_text_sensor",
        CONF_ROOM_THERMOSTAT_BUTTON_TEXT: "set_room_thermostat_button_text_sensor",
        CONF_IGNITION_FAN_BUTTON_TEXT: "set_ignition_fan_button_text_sensor",
        CONF_FEED_BUTTON_TEXT: "set_feed_button_text_sensor",
        CONF_REVERSE_BUTTON_TEXT: "set_reverse_button_text_sensor",
        CONF_SCREENSAVER_BUTTON_TEXT: "set_screensaver_button_text_sensor",

        CONF_PASSWORD_KEYMAP: "set_password_keymap_sensor",
    }
    for key, setter in text_setters.items():
        if key in config:
            ts = await text_sensor.new_text_sensor(config[key])
            cg.add(getattr(var, setter)(ts))

    if CONF_REFRESH in config:
        b = await button.new_button(config[CONF_REFRESH])
        cg.add(b.set_parent(var))

    for ctl in config.get(CONF_CONTROLS, []):
        b = await button.new_button(ctl)
        cg.add(b.set_parent(var))
        cg.add(b.set_page(ctl[CONF_PAGE]))
        cg.add(b.set_component_id(ctl[CONF_COMPONENT_ID]))
