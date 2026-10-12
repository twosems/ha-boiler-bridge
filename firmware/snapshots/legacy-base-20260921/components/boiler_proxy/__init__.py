import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, sensor, text_sensor, button
from esphome.const import CONF_ID, DEVICE_CLASS_TEMPERATURE, UNIT_CELSIUS, UNIT_AMPERE, STATE_CLASS_MEASUREMENT

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor", "text_sensor", "button"]

boiler_proxy_ns = cg.esphome_ns.namespace("boiler_proxy")
BoilerProxy = boiler_proxy_ns.class_("BoilerProxy", cg.Component)
BoilerProxyButton = boiler_proxy_ns.class_("BoilerProxyButton", button.Button)

CONF_DISPLAY_UART_ID = "display_uart_id"
CONF_BOILER_UART_ID = "boiler_uart_id"
CONF_CURRENT_PAGE = "current_page"
CONF_CURRENT_SCREEN = "current_screen"
CONF_BOILER_SETPOINT = "boiler_setpoint"
CONF_DHW_SETPOINT = "dhw_setpoint"
CONF_BOILER_TEMPERATURE = "boiler_temperature"
CONF_DHW_TEMPERATURE = "dhw_temperature"
CONF_REVERSE_TRIP_CURRENT = "reverse_trip_current"
CONF_MOTOR_CURRENT = "motor_current"
CONF_PID_KP = "pid_kp"
CONF_PID_KI = "pid_ki"
CONF_PID_KD = "pid_kd"
CONF_REFRESH = "refresh"

TEMP_SCHEMA = sensor.sensor_schema(unit_of_measurement=UNIT_CELSIUS, accuracy_decimals=1, device_class=DEVICE_CLASS_TEMPERATURE, state_class=STATE_CLASS_MEASUREMENT)
AMP_SCHEMA = sensor.sensor_schema(unit_of_measurement=UNIT_AMPERE, accuracy_decimals=1, state_class=STATE_CLASS_MEASUREMENT)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(BoilerProxy),
    cv.Required(CONF_DISPLAY_UART_ID): cv.use_id(uart.UARTComponent),
    cv.Required(CONF_BOILER_UART_ID): cv.use_id(uart.UARTComponent),
    cv.Optional(CONF_CURRENT_PAGE): sensor.sensor_schema(accuracy_decimals=0),
    cv.Optional(CONF_CURRENT_SCREEN): text_sensor.text_sensor_schema(),
    cv.Optional(CONF_BOILER_SETPOINT): TEMP_SCHEMA,
    cv.Optional(CONF_DHW_SETPOINT): TEMP_SCHEMA,
    cv.Optional(CONF_BOILER_TEMPERATURE): TEMP_SCHEMA,
    cv.Optional(CONF_DHW_TEMPERATURE): TEMP_SCHEMA,
    cv.Optional(CONF_REVERSE_TRIP_CURRENT): AMP_SCHEMA,
    cv.Optional(CONF_MOTOR_CURRENT): AMP_SCHEMA,
    cv.Optional(CONF_PID_KP): sensor.sensor_schema(accuracy_decimals=0),
    cv.Optional(CONF_PID_KI): sensor.sensor_schema(accuracy_decimals=0),
    cv.Optional(CONF_PID_KD): sensor.sensor_schema(accuracy_decimals=0),
    cv.Optional(CONF_REFRESH): button.button_schema(BoilerProxyButton),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    display_uart = await cg.get_variable(config[CONF_DISPLAY_UART_ID])
    boiler_uart = await cg.get_variable(config[CONF_BOILER_UART_ID])
    cg.add(var.set_display_uart(display_uart))
    cg.add(var.set_boiler_uart(boiler_uart))

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
    }
    for key, setter in sensor_setters.items():
        if key in config:
            s = await sensor.new_sensor(config[key])
            cg.add(getattr(var, setter)(s))

    if CONF_CURRENT_SCREEN in config:
        ts = await text_sensor.new_text_sensor(config[CONF_CURRENT_SCREEN])
        cg.add(var.set_current_screen_sensor(ts))

    if CONF_REFRESH in config:
        b = await button.new_button(config[CONF_REFRESH])
        cg.add(b.set_parent(var))
