import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID, CONF_TYPE

from . import ELM327BLEHub, CONF_ELM327_BLE_ID

CONF_DTC = "dtc"
CONF_RAW = "raw"

TEXT_SENSOR_TYPES = {
    CONF_DTC: {
        "name": "Aktive Fehlercodes",
        "icon": "mdi:alert-circle",
    },
    CONF_RAW: {
        "name": "Letzte ELM327 Antwort",
        "icon": "mdi:message-text",
    },
}

CONFIG_SCHEMA = text_sensor.text_sensor_schema().extend(
    {
        cv.GenerateID(CONF_ELM327_BLE_ID): cv.use_id(ELM327BLEHub),
        cv.Required(CONF_TYPE): cv.one_of(*TEXT_SENSOR_TYPES, lower=True),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_ELM327_BLE_ID])
    var = await text_sensor.new_text_sensor(config)

    sensor_type = config[CONF_TYPE]
    if sensor_type == CONF_DTC:
        cg.add(hub.register_dtc_text_sensor(var))
    elif sensor_type == CONF_RAW:
        cg.add(hub.register_raw_text_sensor(var))
