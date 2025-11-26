#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace ens220 {

class ens220 : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  void set_pressure_sensor(sensor::Sensor *pressure_sensor) { pressure_ = pressure_sensor; }

 protected:
  sensor::Sensor *pressure_{nullptr};

 private:
  uint8_t compute_crc8(unsigned char *data, size_t length, unsigned char polynomial, unsigned char init_value);
};

}  // namespace ens220
}  // namespace esphome
