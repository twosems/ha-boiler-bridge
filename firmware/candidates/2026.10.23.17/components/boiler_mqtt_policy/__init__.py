import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import mqtt
DEPENDENCIES=['mqtt']
CONFIG_SCHEMA=cv.Schema({cv.Required('diagnostics'):cv.use_id(mqtt.MQTTTextSensor)})
async def to_code(config):
 var=await cg.get_variable(config['diagnostics'])
 cg.add(var.set_diagnostic_json_attributes(True))
