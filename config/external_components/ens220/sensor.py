import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_EVENT,
    CONF_ID,
    CONF_PRESSURE,
    DEVICE_CLASS_PRESSURE,
    ICON_EMPTY,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_NONE,
    UNIT_EMPTY,
    UNIT_PASCAL,
)

DEPENDENCIES = ["i2c"]

ens220_ns = cg.esphome_ns.namespace("ens220")
ens220 = ens220_ns.class_("ens220", cg.PollingComponent, i2c.I2CDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ens220),
            cv.Required(CONF_PRESSURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_PASCAL,
                device_class=DEVICE_CLASS_PRESSURE,
                state_class=STATE_CLASS_MEASUREMENT,
                icon=ICON_EMPTY,
                accuracy_decimals=1,
            ),
            cv.Required(CONF_EVENT): sensor.sensor_schema(
                unit_of_measurement=UNIT_EMPTY,
                # device_class=DEVICE_CLASS_OPENING,
                state_class=STATE_CLASS_NONE,
                icon=ICON_EMPTY,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x20))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    pressure = config.get(CONF_PRESSURE)
    sensPressure = await sensor.new_sensor(pressure)
    cg.add(var.set_pressure_sensor(sensPressure))

    event = config.get(CONF_EVENT)
    sensEvent = await sensor.new_sensor(event)
    cg.add(var.set_event_sensor(sensEvent))
