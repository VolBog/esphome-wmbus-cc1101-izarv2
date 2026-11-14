import esphome.codegen as cg
from esphome import config_validation as cv
from esphome import pins
from esphome.components import spi

from .transceiver import CONF_DIO1_PIN, CONF_RADIO_ID
from .radio import radio_ns
from esphome.const import CONF_RESET_PIN

CONF_FREQUENCY = "frequency"


def config_schema(radio_type, radio_schema):
    # CC1101 has no hardware reset pin (uses software SRES), so reset_pin is
    # optional here. The IoTLabs base schema requires it for SX parts.
    schema = (
        cv.Schema(
            {
                cv.GenerateID(CONF_RADIO_ID): cv.declare_id(
                    radio_ns.class_(
                        "CC1101",
                        radio_ns.class_("Transceiver"),
                        spi.SPIDevice,
                        cg.Component,
                    )
                ),
                cv.Optional(CONF_RESET_PIN): pins.internal_gpio_output_pin_schema,
                cv.Required(CONF_DIO1_PIN): pins.internal_gpio_input_pin_schema,
                cv.Optional(CONF_FREQUENCY, default="868.95MHz"): cv.All(
                    cv.frequency,
                    cv.Range(min=300e6, max=928e6),
                ),
            }
        )
        .extend(spi.spi_device_schema())
        .extend(cv.COMPONENT_SCHEMA)
    )
    return radio_schema.extend(schema)


async def to_code(config):
    cg.add(cg.LineComment("wM-Bus CC1101 Transceiver"))

    var = await cg.register_component(cg.new_Pvariable(config[CONF_RADIO_ID]), config)

    if CONF_RESET_PIN in config:
        reset_pin = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(reset_pin))

    dio1_pin = await cg.gpio_pin_expression(config[CONF_DIO1_PIN])
    cg.add(var.set_dio1_pin(dio1_pin))

    await spi.register_spi_device(var, config)

    cg.add(var.set_frequency_hz(int(config[CONF_FREQUENCY])))

    return var
