#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"

#include "mprls.h"

#include <string>

namespace esphome {
namespace mprls {

static const char *TAG = "mprls.sensor";
uint8_t id = 0x28;  // i2c address

double press_counts = 0;  // digital pressure reading [counts] double
double pressure =
    0;  // pressure reading [bar, psi, kPa, etc.] outputmax = 15099494; // output at maximum pressure [counts]
double outputmin = 1677722;  // output at minimum pressure [counts]
double pmax = 1;             // maximum value of pressure range [bar, psi, kPa, etc.]
double pmin = 0;             // minimum value of pressure range [bar, psi, kPa, etc.]
double percentage = 0;       // holds percentage of full scale data

void mprls::setup() {
  ESP_LOGI(TAG, "Setup MPRLS i2c address 0x%02X", get_i2c_address());

  this->set_i2c_address(MPRLS_DEFAULT_ADDR);

  PressureMin = 0;
  PressureMax = 5.80103;
  OUTPUT_min = (uint32_t) ((float) COUNTS_224 * (OUTPUT_min / 100.0) + 0.5);
  OUTPUT_max = (uint32_t) ((float) COUNTS_224 * (OUTPUT_max / 100.0) + 0.5);

  ESP_LOGI(TAG, "Max pressure %f, Min pressure %f", PressureMax, PressureMin);
}

void mprls::update() {
  uint8_t cmd[3] = {0x00, 0x00};  // command to be sent

  auto ret = this->write_register(0xAA, cmd, sizeof(cmd));

  if (ret != ::esphome::i2c::ERROR_OK) {
    ESP_LOGW(TAG, "mprls write register failed with error %d", ret);
    return;
  }

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

    ESP_LOGD(TAG, "Attempt %d: statusCode = %d", attempt + 1, statusCode);

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
    ESP_LOGW(TAG, "statusCode still 5 after %d attempts", max_attempts);
    return;
  }

  uint32_t raw_psi = readData();

  if (raw_psi == 0xFFFFFFFF || OUTPUT_min == OUTPUT_max) {
    this->status_set_warning("Failed to read mprls");
    return;
  }

  // All is good, calculate and convert to desired units using provided factor
  // use the 10-90 calibration curve by default or whatever provided by the user
  float psi = (raw_psi - OUTPUT_min) * (PressureMax - PressureMin);
  psi /= (float) (OUTPUT_max - OUTPUT_min);
  psi += PressureMin;

  // convert to desired units

  this->pressure_->publish_state(psi);
  //    this->event_->publish_state(mprlsDriver::state.event_detected);
  this->status_clear_warning();
}

uint32_t mprls::readData(void) {
  uint8_t buffer[4];  // holds output data
  auto ret = this->read(buffer, sizeof(buffer));

  if (ret != ::esphome::i2c::ERROR_OK) {
    ESP_LOGW(TAG, "MPRLS data Read failed with error %d", ret);
    this->status_set_warning("Failed to read mprls");
    return 0xFFFFFFFF;
  }

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
  ESP_LOGI(TAG, "Max pressure set to %f", max_pressure);
  PressureMax = max_pressure;
}

void mprls::set_pressure_min(float min_pressure) {
  // Implementation here
  ESP_LOGI(TAG, "Min pressure set to %f", min_pressure);
  PressureMin = min_pressure;
}

void mprls::dump_config() {}

}  // namespace mprls
}  // namespace esphome
