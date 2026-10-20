import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
DEPENDENCIES=['web_server','esp32']
AUTO_LOAD=['web_server_base']
ns=cg.esphome_ns.namespace('boiler_web_auth')
BoilerWebAuth=ns.class_('BoilerWebAuth',cg.Component)
CONFIG_SCHEMA=cv.Schema({cv.GenerateID():cv.declare_id(BoilerWebAuth),cv.Required('username'):cv.string_strict,cv.Required('password'):cv.string_strict}).extend(cv.COMPONENT_SCHEMA)
async def to_code(config):
 var=cg.new_Pvariable(config[CONF_ID]);await cg.register_component(var,config)
 cg.add(var.set_defaults(config['username'],config['password']))
