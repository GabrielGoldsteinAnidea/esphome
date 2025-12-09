#include "mprls.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"

#include <string>

namespace esphome {
namespace mprls {

static const char *TAG = "mprls.sensor";

void mprls::setup() {
  ESP_LOGCONFIG(TAG, "Setup MPRLS i2c address 0x%02X", get_i2c_address());

  this->set_i2c_address(MPRLS_DEFAULT_ADDR);

  // PressureMin = 0;
  // PressureMax = 5.80103;

  ESP_LOGCONFIG(TAG, "Max pressure %f, Min pressure %f", PressureMax, PressureMin);
}

void mprls::update() {
  ESP_LOGV(TAG, "Output Min %.5f, max %.5f", OUTPUT_min, OUTPUT_max);
  ESP_LOGV(TAG, "Pressure Min %.5f, max %.5f", PressureMin, PressureMax);

  uint8_t statusCode;
  int max_attempts = 10;
  int attempt = 0;

  // Loop until statusCode is not 5 (low), max 10 times with 1ms delay
  while (attempt < max_attempts) {
    auto ret = this->read(&statusCode, sizeof(statusCode));

    if (ret != ::esphome::i2c::ERROR_OK) {
      ESP_LOGW(TAG, "MPRLS Read failed with error %d", ret);
      return;
    }

    // ESP_LOGD(TAG, "Attempt %d: statusCode = %d", attempt + 1, statusCode);

    if (statusCode & MPRLS_STATUS_BUSY) {
      // Status code is ready (not 5)
      ESP_LOGD(TAG, "Status code ready after %d attempts", attempt + 1);
      break;
    }

    attempt++;
    if (attempt < max_attempts) {
      delayMicroseconds(1000);  // 1ms delay
    }
  }

  if (statusCode & MPRLS_STATUS_BUSY) {
    ESP_LOGW(TAG, "statusCode still busy after %d attempts", max_attempts);
    return;
  }

  uint32_t raw_psi = readData();

  if (raw_psi == 0xFFFFFFFF || OUTPUT_min == OUTPUT_max) {
    this->status_set_warning("Failed to read mprls");
    return;
  }

  ESP_LOGV(TAG, "Raw reading %u", raw_psi);

  // All is good, calculate and convert to desired units using provided factor
  // use the 10-90 calibration curve by default or whatever provided by the user
  float pressure = (raw_psi - OUTPUT_min) * (PressureMax - PressureMin);

  pressure /= (float) (OUTPUT_max - OUTPUT_min);
  pressure += PressureMin;

  this->pressure_->publish_state(pressure);
  this->status_clear_warning();

  // Trigger next read
  uint8_t cmd[2] = {0x00, 0x00};  // command to be sent
  auto ret = this->write_register(READ_CMD, cmd, sizeof(cmd));

  if (ret != ::esphome::i2c::ERROR_OK) {
    ESP_LOGW(TAG, "mprls write register failed with error %d", ret);
    this->status_set_warning("Failed to trigger mprls next read");
    return;
  }

  return;
}

uint32_t mprls::readData(void) {
  uint8_t buffer[4];  // holds output data
  auto ret = this->read(buffer, sizeof(buffer));

  if (ret != ::esphome::i2c::ERROR_OK) {
    ESP_LOGW(TAG, "MPRLS data Read failed with error %d", ret);
    this->status_set_warning("Failed to read mprls readData");
    return 0xFFFFFFFF;
  }

  // ESP_LOGI(TAG, "MPRLS Read");

  // check status byte
  if (buffer[0] & MPRLS_STATUS_MATHSAT) {
    ESP_LOGW(TAG, "MPRLS math saturation error");
    this->status_set_warning("MPRLS math saturation error");
    return 0xFFFFFFFF;
  }
  if (buffer[0] & MPRLS_STATUS_FAILED) {
    ESP_LOGW(TAG, "MPRLS status failed");
    this->status_set_warning("MPRLS status failed");
    return 0xFFFFFFFF;
  }

  // all good, return data
  return (uint32_t(buffer[1]) << 16) | (uint32_t(buffer[2]) << 8) | (uint32_t(buffer[3]));
}

void mprls::set_pressure_max(float max_pressure) {
  // Implementation here
  ESP_LOGCONFIG(TAG, "Max pressure set to %f", max_pressure);
  PressureMax = max_pressure;
}

void mprls::set_pressure_min(float min_pressure) {
  // Implementation here
  ESP_LOGCONFIG(TAG, "Min pressure set to %f", min_pressure);
  PressureMin = min_pressure;
}

void mprls::set_output_max(float max_output) {
  // Implementation here
  ESP_LOGCONFIG(TAG, "Output max set to %f", max_output);

  OUTPUT_max = (uint32_t) ((float) COUNTS_224 * (max_output / 100.0) + 0.5);
  ESP_LOGCONFIG(TAG, "OUTPUT_max counts set to %u", OUTPUT_max);
}

void mprls::set_output_min(float min_output) {
  // Implementation here
  ESP_LOGCONFIG(TAG, "Output min set to %f", min_output);
  OUTPUT_min = (uint32_t) ((float) COUNTS_224 * (min_output / 100.0) + 0.5);
  ESP_LOGCONFIG(TAG, "OUTPUT_min counts set to %u", OUTPUT_min);
}

void mprls::dump_config() {
  ESP_LOGCONFIG(TAG, "MPRLS:\n");
  LOG_I2C_DEVICE(this);

  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }

  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("   ", "Output Min %.5f", (float) OUTPUT_min);
  LOG_SENSOR("   ", "Output Max %.5f", (float) OUTPUT_max);
  LOG_SENSOR("   ", "Pressure Min %.5f", (float) PressureMin);
  LOG_SENSOR("   ", "Pressure Max %.5f", (float) PressureMax);
}

float mprls::get_setup_priority() const { return setup_priority::DATA; }
}  // namespace mprls
}  // namespace esphome
