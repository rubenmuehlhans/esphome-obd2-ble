import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_client
from esphome.const import CONF_ID

CODEOWNERS = ["@rubenmuehlhans"]
DEPENDENCIES = ["ble_client"]
AUTO_LOAD = ["sensor", "text_sensor", "binary_sensor"]
MULTI_CONF = True

CONF_ELM327_BLE_ID = "elm327_ble_id"
CONF_SERVICE_UUID = "service_uuid"
CONF_CHAR_TX_UUID = "char_tx_uuid"
CONF_CHAR_RX_UUID = "char_rx_uuid"
CONF_REQUEST_INTERVAL = "request_interval"
CONF_REQUEST_TIMEOUT = "request_timeout"

elm327_ble_ns = cg.esphome_ns.namespace("elm327_ble")
ELM327BLEHub = elm327_ble_ns.class_(
    "ELM327BLEHub", cg.Component, ble_client.BLEClientNode
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ELM327BLEHub),
            cv.Required(CONF_SERVICE_UUID): cv.string,
            cv.Required(CONF_CHAR_TX_UUID): cv.string,
            cv.Required(CONF_CHAR_RX_UUID): cv.string,
            cv.Optional(
                CONF_REQUEST_INTERVAL, default="2s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_REQUEST_TIMEOUT, default="5s"
            ): cv.positive_time_period_milliseconds,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ble_client.BLE_CLIENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)

    cg.add(var.set_service_uuid(config[CONF_SERVICE_UUID]))
    cg.add(var.set_char_tx_uuid(config[CONF_CHAR_TX_UUID]))
    cg.add(var.set_char_rx_uuid(config[CONF_CHAR_RX_UUID]))
    cg.add(var.set_request_interval(config[CONF_REQUEST_INTERVAL]))
    cg.add(var.set_request_timeout(config[CONF_REQUEST_TIMEOUT]))
