import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_ID

from . import elm327_ble_ns, ELM327BLEHub, CONF_ELM327_BLE_ID

ELM327BLESwitch = elm327_ble_ns.class_("ELM327BLESwitch", switch.Switch)

CONFIG_SCHEMA = switch.switch_schema(
    ELM327BLESwitch,
    icon="mdi:bluetooth-connect",
).extend(
    {
        cv.GenerateID(CONF_ELM327_BLE_ID): cv.use_id(ELM327BLEHub),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_ELM327_BLE_ID])
    var = await switch.new_switch(config)

    cg.add(var.set_hub(hub))
    cg.add(hub.register_connection_switch(var))
