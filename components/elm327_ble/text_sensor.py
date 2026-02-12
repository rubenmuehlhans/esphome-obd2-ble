import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID, CONF_TYPE

from . import ELM327BLEHub, CONF_ELM327_BLE_ID

CONF_DTC = "dtc"
CONF_RAW = "raw"
CONF_RAW_PID = "raw_pid"
CONF_PID = "pid"
CONF_MODE = "mode"
CONF_HEADER = "header"
CONF_COMMAND = "command"

TEXT_SENSOR_TYPES = {
    CONF_DTC: {
        "name": "Aktive Fehlercodes",
        "icon": "mdi:alert-circle",
    },
    CONF_RAW: {
        "name": "Letzte ELM327 Antwort",
        "icon": "mdi:message-text",
    },
    CONF_RAW_PID: {
        "name": "Raw PID Antwort",
        "icon": "mdi:hexadecimal",
    },
}


def validate_raw_pid(config):
    """raw_pid braucht entweder pid+mode oder command."""
    if config[CONF_TYPE] == CONF_RAW_PID:
        has_pid = CONF_PID in config
        has_cmd = CONF_COMMAND in config
        if not has_pid and not has_cmd:
            raise cv.Invalid(
                "raw_pid braucht entweder 'pid' (mit optionalem 'mode') "
                "oder 'command'"
            )
        if has_pid and has_cmd:
            raise cv.Invalid(
                "raw_pid: 'pid' und 'command' sind gegenseitig "
                "ausschliessend"
            )
    return config


CONFIG_SCHEMA = cv.All(
    text_sensor.text_sensor_schema().extend(
        {
            cv.GenerateID(CONF_ELM327_BLE_ID): cv.use_id(ELM327BLEHub),
            cv.Required(CONF_TYPE): cv.one_of(*TEXT_SENSOR_TYPES, lower=True),
            cv.Optional(CONF_MODE, default=0x22): cv.hex_uint8_t,
            cv.Optional(CONF_PID): cv.hex_uint16_t,
            cv.Optional(CONF_HEADER): cv.string,
            cv.Optional(CONF_COMMAND): cv.string,
        }
    ),
    validate_raw_pid,
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_ELM327_BLE_ID])
    var = await text_sensor.new_text_sensor(config)

    sensor_type = config[CONF_TYPE]
    if sensor_type == CONF_DTC:
        cg.add(hub.register_dtc_text_sensor(var))
    elif sensor_type == CONF_RAW:
        cg.add(hub.register_raw_text_sensor(var))
    elif sensor_type == CONF_RAW_PID:
        header = config.get(CONF_HEADER, "")
        if CONF_COMMAND in config:
            # Beliebiger Befehl (z.B. "2101\r" oder "ATSH7E4\r")
            cg.add(
                hub.register_raw_pid_text_sensor(
                    var, 0, 0, header, config[CONF_COMMAND]
                )
            )
        else:
            # Mode + PID → Command wird im C++ generiert
            mode = config[CONF_MODE]
            pid = config[CONF_PID]
            cg.add(hub.register_raw_pid_text_sensor(var, mode, pid, header, ""))
