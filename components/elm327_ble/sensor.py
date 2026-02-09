import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    CONF_ICON,
    CONF_UNIT_OF_MEASUREMENT,
    CONF_ACCURACY_DECIMALS,
    CONF_DEVICE_CLASS,
    STATE_CLASS_MEASUREMENT,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    DEVICE_CLASS_PRESSURE,
    DEVICE_CLASS_SPEED,
    UNIT_CELSIUS,
    UNIT_PERCENT,
    UNIT_VOLT,
)

from . import ELM327BLEHub, CONF_ELM327_BLE_ID, elm327_ble_ns

CONF_PID = "pid"
CONF_MODE = "mode"
CONF_AT_COMMAND = "at_command"
CONF_TYPE = "type"

# Vordefinierte PID-Typen mit Standardwerten
PID_TYPES = {
    "coolant_temp": {
        "name": "Motortemperatur",
        "pid": 0x05,
        "unit": UNIT_CELSIUS,
        "accuracy": 0,
        "device_class": DEVICE_CLASS_TEMPERATURE,
        "icon": "mdi:thermometer",
    },
    "rpm": {
        "name": "Drehzahl",
        "pid": 0x0C,
        "unit": "RPM",
        "accuracy": 0,
        "device_class": "",
        "icon": "mdi:engine",
    },
    "speed": {
        "name": "Geschwindigkeit",
        "pid": 0x0D,
        "unit": "km/h",
        "accuracy": 0,
        "device_class": "",
        "icon": "mdi:speedometer",
    },
    "engine_load": {
        "name": "Motorlast",
        "pid": 0x04,
        "unit": UNIT_PERCENT,
        "accuracy": 1,
        "device_class": "",
        "icon": "mdi:gauge",
    },
    "intake_temp": {
        "name": "Ansauglufttemperatur",
        "pid": 0x0F,
        "unit": UNIT_CELSIUS,
        "accuracy": 0,
        "device_class": DEVICE_CLASS_TEMPERATURE,
        "icon": "mdi:air-filter",
    },
    "fuel_level": {
        "name": "Kraftstoffstand",
        "pid": 0x2F,
        "unit": UNIT_PERCENT,
        "accuracy": 1,
        "device_class": "",
        "icon": "mdi:gas-station",
    },
    "throttle": {
        "name": "Drosselklappe",
        "pid": 0x11,
        "unit": UNIT_PERCENT,
        "accuracy": 1,
        "device_class": "",
        "icon": "mdi:car-cruise-control",
    },
    "battery_voltage": {
        "name": "Batteriespannung",
        "pid": 0x00,
        "at_command": "ATRV\r",
        "unit": UNIT_VOLT,
        "accuracy": 1,
        "device_class": DEVICE_CLASS_VOLTAGE,
        "icon": "mdi:car-battery",
    },
    "intake_map": {
        "name": "Ansaugkrümmerdruck",
        "pid": 0x0B,
        "unit": "kPa",
        "accuracy": 0,
        "device_class": DEVICE_CLASS_PRESSURE,
        "icon": "mdi:gauge-low",
    },
    "maf": {
        "name": "Luftmassenmesser",
        "pid": 0x10,
        "unit": "g/s",
        "accuracy": 2,
        "device_class": "",
        "icon": "mdi:weather-windy",
    },
    "engine_runtime": {
        "name": "Motorlaufzeit",
        "pid": 0x1F,
        "unit": "s",
        "accuracy": 0,
        "device_class": "",
        "icon": "mdi:timer-outline",
    },
    "oil_temp": {
        "name": "Motoröltemperatur",
        "pid": 0x5C,
        "unit": UNIT_CELSIUS,
        "accuracy": 0,
        "device_class": DEVICE_CLASS_TEMPERATURE,
        "icon": "mdi:oil-temperature",
    },
    "ambient_temp": {
        "name": "Umgebungstemperatur",
        "pid": 0x46,
        "unit": UNIT_CELSIUS,
        "accuracy": 0,
        "device_class": DEVICE_CLASS_TEMPERATURE,
        "icon": "mdi:thermometer",
    },
    "ecu_voltage": {
        "name": "ECU Spannung",
        "pid": 0x42,
        "unit": UNIT_VOLT,
        "accuracy": 3,
        "device_class": DEVICE_CLASS_VOLTAGE,
        "icon": "mdi:flash",
    },
    "fuel_rate": {
        "name": "Kraftstoffverbrauch",
        "pid": 0x5E,
        "unit": "L/h",
        "accuracy": 2,
        "device_class": "",
        "icon": "mdi:fuel",
    },
    "baro_pressure": {
        "name": "Barometrischer Druck",
        "pid": 0x33,
        "unit": "kPa",
        "accuracy": 0,
        "device_class": DEVICE_CLASS_PRESSURE,
        "icon": "mdi:weather-partly-cloudy",
    },
    "egr": {
        "name": "Abgasrückführung",
        "pid": 0x2E,
        "unit": UNIT_PERCENT,
        "accuracy": 1,
        "device_class": "",
        "icon": "mdi:recycle",
    },
}


def validate_pid_sensor(config):
    """Setzt Standardwerte basierend auf dem PID-Typ."""
    if CONF_TYPE in config:
        pid_type = config[CONF_TYPE]
        if pid_type in PID_TYPES:
            defaults = PID_TYPES[pid_type]
            if CONF_NAME not in config:
                config[CONF_NAME] = defaults["name"]
            if CONF_PID not in config and "at_command" not in defaults:
                config[CONF_PID] = defaults["pid"]
            if CONF_AT_COMMAND not in config and "at_command" in defaults:
                config[CONF_AT_COMMAND] = defaults["at_command"]
            if CONF_UNIT_OF_MEASUREMENT not in config:
                config[CONF_UNIT_OF_MEASUREMENT] = defaults["unit"]
            if CONF_ACCURACY_DECIMALS not in config:
                config[CONF_ACCURACY_DECIMALS] = defaults["accuracy"]
            if CONF_ICON not in config:
                config[CONF_ICON] = defaults["icon"]
            if CONF_DEVICE_CLASS not in config and defaults["device_class"]:
                config[CONF_DEVICE_CLASS] = defaults["device_class"]
    return config


CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.GenerateID(CONF_ELM327_BLE_ID): cv.use_id(ELM327BLEHub),
            cv.Optional(CONF_TYPE): cv.one_of(*PID_TYPES, lower=True),
            cv.Optional(CONF_MODE, default=0x01): cv.hex_uint8_t,
            cv.Optional(CONF_PID): cv.hex_uint8_t,
            cv.Optional(CONF_AT_COMMAND): cv.string,
        }
    ),
    validate_pid_sensor,
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_ELM327_BLE_ID])
    var = await sensor.new_sensor(config)

    if CONF_AT_COMMAND in config:
        cg.add(hub.register_at_sensor(var, config[CONF_AT_COMMAND]))
    elif CONF_PID in config:
        cg.add(hub.register_pid_sensor(var, config[CONF_MODE], config[CONF_PID]))
