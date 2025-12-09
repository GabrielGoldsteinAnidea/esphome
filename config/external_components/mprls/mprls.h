#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace mprls {

#define MPRLS_DEFAULT_ADDR (0x18)
#define MPRLS_READ_TIMEOUT (20)
#define MPRLS_STATUS_POWERED (0x40)
#define MPRLS_STATUS_BUSY (0x20)
#define MPRLS_STATUS_FAILED (0x04)
#define MPRLS_STATUS_MATHSAT (0x01)
#define COUNTS_224 (16777216L)
#define PSI_to_HPA (68.947572932)
#define MPRLS_STATUS_MASK (0b01100101)
#define READ_CMD 0xAA

class mprls : public sensor::Sensor,
              public binary_sensor::BinarySensor,
              public PollingComponent,
              public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  void set_pressure_sensor(sensor::Sensor *pressure_sensor) { pressure_ = pressure_sensor; }
  //  void set_event_sensor(binary_sensor::BinarySensor *event_sensor) { event_ = event_sensor; }

  float get_setup_priority() const override;

  void set_pressure_max(float max_pressure);
  void set_pressure_min(float min_pressure);

  void set_output_max(float max_output);
  void set_output_min(float min_output);

 protected:
  sensor::Sensor *pressure_{nullptr};

  //  binary_sensor::BinarySensor *event_{nullptr};
 private:
  uint32_t readData(void);

  double PressureMin, PressureMax;
  double OUTPUT_min, OUTPUT_max;
};

}  // namespace mprls
}  // namespace esphome
