import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor, wifi
from esphome.const import CONF_ID
DEPENDENCIES=['wifi','esp32']
AUTO_LOAD=['text_sensor']
ns=cg.esphome_ns.namespace('boiler_network')
BoilerNetwork=ns.class_('BoilerNetwork',cg.Component)
CONFIG_SCHEMA=cv.Schema({cv.GenerateID():cv.declare_id(BoilerNetwork),
  cv.Required('status'):text_sensor.text_sensor_schema(),
  cv.Required('networks'):text_sensor.text_sensor_schema()}).extend(cv.COMPONENT_SCHEMA)
async def to_code(config):
  wifi.request_wifi_scan_results_listener()
  var=cg.new_Pvariable(config[CONF_ID]);await cg.register_component(var,config)
  for key in ('status','networks'):
    s=await text_sensor.new_text_sensor(config[key]);cg.add(getattr(var,'set_'+key)(s))
