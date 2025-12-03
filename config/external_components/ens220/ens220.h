#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace ens220 {

class ens220 : public sensor::Sensor,
               public binary_sensor::BinarySensor,
               public PollingComponent,
               public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  void set_pressure_sensor(sensor::Sensor *pressure_sensor) { pressure_ = pressure_sensor; }
  void set_event_sensor(binary_sensor::BinarySensor *event_sensor) { event_ = event_sensor; }

 protected:
  sensor::Sensor *pressure_{nullptr};

  binary_sensor::BinarySensor *event_{nullptr};
};

}  // namespace ens220
}  // namespace esphome
