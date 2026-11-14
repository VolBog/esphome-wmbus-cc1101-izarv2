import esphome.codegen as cg
from esphome import config_validation as cv

from .transceiver import register_transceiver, transceiver_schema

CONF_FREQUENCY = "frequency"


def config_schema(radio_type, radio_schema):
    # CC1101 has no hardware reset pin (it uses a software SRES strobe), so
    # reset_pin is left unset; the base transceiver_schema makes it optional.
    return radio_schema.extend(
        transceiver_schema(
            radio_type,
            {
                cv.Optional(CONF_FREQUENCY, default="868.95MHz"): cv.All(
                    cv.frequency, cv.Range(min=300e6, max=928e6)
                ),
            },
        )
    )


async def to_code(config):
    var = await register_transceiver(config)

    cg.add(var.set_frequency_hz(int(config[CONF_FREQUENCY])))

    return var
