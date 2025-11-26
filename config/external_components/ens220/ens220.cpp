#include "esphome/core/log.h"
#include "ens220.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace ens220 {

static const char *TAG = "ens220.sensor";

unsigned char ens220::compute_crc8(unsigned char *data, size_t length, unsigned char polynomial,
                                   unsigned char init_value) {
  unsigned char crc = init_value;

  for (size_t i = 0; i < length; i++) {
    crc ^= data[i];  // XOR the byte with the current CRC value
    for (int bit = 0; bit < 8; bit++) {
      if (crc & 0x80) {                 // If the top bit is 1
        crc = (crc << 1) ^ polynomial;  // Shift left and XOR with polynomial
      } else {
        crc <<= 1;  // Just shift left
      }
      crc &= 0xFF;  // Ensure CRC is 8 bits
    }
  }

  return crc;
}

void ens220::setup() {
  uint8_t command[] = {0xFE};
  this->write(command, sizeof(command), true);
}

void ens220::update() {
  uint8_t command[] = {0xF1};
  this->write(command, sizeof(command), false);

  uint8_t data[3];
  if (auto b = this->read_bytes_raw(data, sizeof(data))) {
    ESP_LOGD(TAG, "Raw data %02X%02X, %02X\n", data[0], data[1], data[2]);

    if (this->pressure_ != nullptr) {
      int16_t rawSigned = (data[0] << 8) | data[1];
      float pressure = rawSigned;

      uint crc = compute_crc8(data, 2, 0x31, 0x00);

      ESP_LOGD(TAG, "CRC %02X, %02X\n", data[2], crc);

      if (data[2] != crc) {
        ESP_LOGW(TAG, "CRC Failed");
        return;
      }

      ESP_LOGD(TAG, "raw int %d, float %f\n", rawSigned, pressure);
      // if ((data[0] == 0xFF) && (data[1] == 0xFF))
      // {
      //     pressure = 0.0f;
      // }

      pressure /= 60.0;  // Scale factor for Pa

      if (std::isnan(pressure)) {
        ESP_LOGW(TAG, "Invalid pressure reading (0%%), ");
      }

      this->pressure_->publish_state(pressure);

      this->status_clear_warning();
    }

  } else {
    ESP_LOGW(TAG, "Fail");
  }
}

void ens220::dump_config() {}

}  // namespace ens220
}  // namespace esphome
