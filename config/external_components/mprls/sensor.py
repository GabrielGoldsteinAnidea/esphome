import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    #    CONF_EVENT,
    CONF_ID,
    CONF_PRESSURE,
    #    DEVICE_CLASS_OPENING,
    DEVICE_CLASS_PRESSURE,
    ICON_EMPTY,
    STATE_CLASS_MEASUREMENT,
    UNIT_PASCAL,
)

DEPENDENCIES = ["i2c"]

mprls_ns = cg.esphome_ns.namespace("mprls")
mprls = mprls_ns.class_("mprls", cg.PollingComponent, i2c.I2CDevice)

CONF_PASCAL_MAX = "maximum_pascal"
CONF_PASCAL_MIN = "minimum_pascal"
CONF_OUTPUT_MAX = "output_max"
CONF_OUTPUT_MIN = "output_min"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(mprls),
            cv.Required(CONF_PRESSURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_PASCAL,
                device_class=DEVICE_CLASS_PRESSURE,
                state_class=STATE_CLASS_MEASUREMENT,
                icon=ICON_EMPTY,
                accuracy_decimals=5,
            ),
            # cv.Optional(CONF_EVENT): binary_sensor.binary_sensor_schema(
            #     device_class=DEVICE_CLASS_OPENING,
            #     icon=ICON_EMPTY,
            # ),
            cv.Optional(CONF_PASCAL_MAX, default=300.0): cv.float_,
            cv.Optional(CONF_PASCAL_MIN, default=0.0): cv.float_,
            cv.Optional(CONF_OUTPUT_MAX, default=95): cv.float_,
            cv.Optional(CONF_OUTPUT_MIN, default=5): cv.float_,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x18))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    pressure = config.get(CONF_PRESSURE)
    sensPressure = await sensor.new_sensor(pressure)
    cg.add(var.set_pressure_sensor(sensPressure))

    # event = config.get(CONF_EVENT)
    # sensEvent = await binary_sensor.new_binary_sensor(event)
    # cg.add(var.set_event_sensor(sensEvent))

    # Access the custom parameters from the config dictionary
    pressure_Max = config[CONF_PASCAL_MAX]
    pressure_Min = config[CONF_PASCAL_MIN]

    # Use these parameters to set properties of your C++ component
    cg.add(var.set_pressure_max(pressure_Max))
    cg.add(var.set_pressure_min(pressure_Min))

    output_Max = config[CONF_OUTPUT_MAX]
    output_Min = config[CONF_OUTPUT_MIN]

    cg.add(var.set_output_max(output_Max))
    cg.add(var.set_output_min(output_Min))
