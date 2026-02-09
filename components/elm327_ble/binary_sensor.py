import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_ID, CONF_TYPE

from . import ELM327BLEHub, CONF_ELM327_BLE_ID

CONF_CONNECTED = "connected"
CONF_ENGINE_RUNNING = "engine_running"

BINARY_SENSOR_TYPES = {
    CONF_CONNECTED: {
        "name": "ELM327 verbunden",
        "icon": "mdi:bluetooth-connect",
    },
    CONF_ENGINE_RUNNING: {
        "name": "Motor läuft",
        "icon": "mdi:engine",
    },
}

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema().extend(
    {
        cv.GenerateID(CONF_ELM327_BLE_ID): cv.use_id(ELM327BLEHub),
        cv.Required(CONF_TYPE): cv.one_of(*BINARY_SENSOR_TYPES, lower=True),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_ELM327_BLE_ID])
    var = await binary_sensor.new_binary_sensor(config)

    sensor_type = config[CONF_TYPE]
    if sensor_type == CONF_CONNECTED:
        cg.add(hub.register_connected_binary_sensor(var))
    elif sensor_type == CONF_ENGINE_RUNNING:
        cg.add(hub.register_engine_running_binary_sensor(var))
