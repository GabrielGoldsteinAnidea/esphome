#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"

#include "ens220.h"

// Include ScioSense library at global scope so its namespace isn't nested
#define ENS220_CPP
#include "sciosense-ens220-arduino/src/ScioSense_ENS220.h"

#include <string>

namespace esphome {
namespace ens220 {
class ens220;
}
}  // namespace esphome

namespace esphome {
namespace ens220 {
static const char *TAG = "ens220.sensor";
}
}  // namespace esphome

namespace ens220Driver {
// Start of file:
// config/external_components/ens220/sciosense-ens220-arduino/examples/05_Event_Detection/05_Event_Detection.ino

// #include <Arduino.h>
// #include <ens220.h>

/////////////////////////////////////////////////////////////////
//                                                             //
//    ENS220: High-accuracy pressure and temperature  sensor   //
//    ------------------------------------------------         //
//                                                             //
//    Application example 1: Indoor (pressure) event detection //
//                                                             //
/////////////////////////////////////////////////////////////////

// This algorithm enables the detection of window and door opening events in a building using the high-precision ENS220
// pressure and temperature sensor

// The ENS220 offers precise air pressure measurements with high resolution and sampling rate at low power consumption.
// Events such as the opening and closing of doors and windows lead to small and rapid changes in air pressure. These
// changes can be clearly detected with the ENS220 due to its high resolution and sampling rate.

// PLEASE READ OUR APPLICATION NOTE TO UNDERSTAND THIS CODE EXAMPLE AND ITS USE CASE:
// https://www.sciosense.com/products/pressure-sensor/ens220/

// Define the I2C address of the ENS220
#define I2C_ADDRESS 0x20  // Default/fixed I2C address of the ENS220

// Define the speed of the Serial communication with the computer
// #define SERIAL_BAUDRATE 57600

// If the following line is un-commented, debug messages will be printed through the Serial port
// #define DEBUG_ENS220

// You can (optionally) connect an LED to visually indicate pressure events
// #define OPTIONAL_LED_PIN 19
bool ledState = LOW;

// Create an ENS220 sensor object
ENS220 ens220;

// Declare/define variables for data readout
unsigned long relative_time_ms;
long last_readout_time_ms = 0;
float absolute_pressure_Pa;
float first_pressure_Pa = 0.0;
float relative_pressure_Pa;  // Absolute Pressure (t) - Absolute Pressure (t_0)
long target_sampling_interval_ms =
    64;  // NOTE: The algorithm parameters are optimized for this interval. Do not change it.

// Define event detection algorithm  parameters
float event_threshold_multiplier = 6.0;  // Event threshold multiplier, event is detected if filtered pressure
// derivative is m times larger than average variation
float minimum_event_duration_s =
    1.5;  // Minimum event duration in seconds, once the filtered pressure derivative is passed the threshold the event
// continues for at least d seconds. This is used to prevent multiple event detection from one event signal
float average_calculation_decay_rate =
    0.9999;  // Decay rate for calculating mean absolute deviation (mad) using exponential moving average. Example:
// average_calculation_decay_rate=1: average with no decaye, average_calculation_decay_rate =0.9999 :
// average approximately 10000 samples
// Structure declaration for holding the states     // These are variables that needs to be presisted if we want to run
// the algo on per sample basis
typedef struct {
  int event_detected;                    // Event detected
  int first_event_lower_than_threshold;  // Indicates first sample in the event where the signal (filtered pressure
  // derivative) becomes smaller than threshold
  int first_event_detected;  // Indicates the first sample that the event is detected
  float norm;                // normalizing constant for average calculation, initital value =1
  float average;  // Average value of filtered derivative of pressure, used for calculating mean absolute deviation
  float mean_absolute_deviation;  // Mean absolute deviation of filtered derivative of pressure, initial value = 2.0
  float p_1;  // Variable for keeping one time lagged sample of pressure p_1 = p[n-1]= p[t- Ts] where t is time and Ts
  // is sample time
  float t_1;                  // Variable for keeping one time lagged value of time t_1 = t- Ts. Thus t-t_1= Ts
  float dp;                   // Derivative of pressure: dp[n]
  float dpf;                  // Derivative of pressure after filtering: dpf[n-1]
  float dp_1;                 // Variable for keeping one time lagged sample of derivative of pressure: dp[n-1]
  float dp_2;                 // Variable for keeping two time lagged sample of derivative of pressure: dp[n-2]
  float dpf_1;                // dpf[n-1]
  float dpf_2;                // dpf[n-2]
  float detection_threshold;  // Threshold for event detection
  float t_first_event_lower_than_threshold;  // Time that signal is first time smaller than threshold
} State;
State state;

void ens220_setup(class esphome::ens220::ens220 *esphomeParent);
void toggle_led();
void turn_led_on();
void turn_led_off();

/////////// ALGORITHM ///////////////
void detect_events(float p, float t, float m, float d, float alpha) {
  // Filter coefficients for filtering derivate of pressure. Second order Butterworth filter
  // dpf[n] = -a[1]* dpf[n-1] -a[2] * dpf[n-2] + b[0] dp[n]+ b[1] dp[n-1]+ b[2] dp[n-2]
  float b[] = {0.06745527, 0.13491055, 0.06745527};
  float a[] = {1.0, -1.1429805, 0.4128016};
  float event_weight_in_average = 1 / 8.0;

  state.dp_2 = state.dp_1;  // Time lag for filtering
  state.dp_1 = state.dp;    // Time lag for filtering
  if ((t - state.t_1) == 0) {
    state.dp = 0;
  } else {
    state.dp = (p - state.p_1) / (t - state.t_1);  // Derivative
  }
  state.p_1 = p;  // Time lag for calculation of derivative
  state.t_1 = t;

  state.dpf_2 = state.dpf_1;  // Time lag for filtering
  state.dpf_1 = state.dpf;    // Time lag for filtering

  state.dpf =
      -a[1] * state.dpf_1 - a[2] * state.dpf_2 + b[0] * state.dp + b[1] * state.dp_1 + b[2] * state.dp_2;  // IIR filter

  state.detection_threshold = state.mean_absolute_deviation * m;  // Threshold calculation

  // Update mean absolute variation
  if (fabs(state.dpf) < state.detection_threshold) {  // update when not in event
    state.norm = state.norm * alpha + 1.0;
    state.average = state.average + (state.dpf - state.average) / state.norm;
    state.mean_absolute_deviation =
        state.mean_absolute_deviation + (fabs(state.dpf - state.average) - state.mean_absolute_deviation) / state.norm;
  } else {  // Update slower during event. To make update during event slower change 8.0 to a larger number.
    state.average = state.average + (state.dpf - state.average) * event_weight_in_average / state.norm;
    state.mean_absolute_deviation =
        state.mean_absolute_deviation +
        (fabs(state.dpf - state.average) - state.mean_absolute_deviation) * event_weight_in_average / state.norm;
  }

  // Event detection
  if (fabs(state.dpf) > state.detection_threshold) {  // larger than threshold
    state.event_detected = 1;
    state.first_event_detected = 1;
    state.first_event_lower_than_threshold = 1;
    if (state.first_event_detected) {
      state.first_event_detected = 0;
    }
  } else {  // smaller than threshold
    if (state.event_detected) {
      if (state.first_event_lower_than_threshold) {
        state.t_first_event_lower_than_threshold = t;
        state.first_event_lower_than_threshold = 0;
      }
      // Event duration d is used to indicate the end-time for an event after signal goes lower than threshold.
      // The signal might go momentarily lower than threshold and go higher again. The time threshold is used so that we
      // don't detect a single event as multiple ones.
      if ((t - state.t_first_event_lower_than_threshold) > d) {
        state.event_detected = 0;
        state.first_event_detected = 1;
      }
    }
  }
}

void setup(class esphome::ens220::ens220 *esphomeParent) {
  // pinMode(OPTIONAL_LED_PIN, OUTPUT);
  // Serial.begin(SERIAL_BAUDRATE);

  // Wire.begin();

// If DEBUG_ENS220 is enabled and we're in an environment with a Serial/Print device,
// enable debugging. For EspHome we provide a small Print wrapper that forwards debug
// output to ESP_LOGD so that debug output appears in the platform logs.
#ifdef DEBUG_ENS220
  // Prefer Serial if available (for native Arduino/e.g. direct Arduino builds)
// Prefer Arduino Serial if available
#if defined(ARDUINO) && defined(Serial)
  // There is no Stream usage in the core API now; to keep Arduino Serial support, callers can
  // install a simple debug function that writes using Serial
  static void arduino_debug_cb(void *userData, const char *msg) {
    (void) userData;
    Serial.println(msg);
  }
  ens220.enableDebugging(nullptr, arduino_debug_cb);
#else
  // EspHome logging callback: forward messages to ESP_LOGD with our TAG
  static void esphome_debug_cb(void *userData, const char *msg) {
    const char *tag = (const char *) userData;
    ESP_LOGD(tag, "%s", msg);
  }
  ens220.enableDebugging((void *) esphome::ens220::TAG, esphome_debug_cb);
#endif
#endif

  ens220_setup(esphomeParent);

  // Initialize algorithm
  relative_time_ms = esphome::millis();
  auto result = ens220.update();
  while (result != RESULT_OK) {
    relative_time_ms = esphome::millis();
    result = ens220.update();
  }
  first_pressure_Pa = ens220.getPressurePascal();
  state = {.event_detected = 0,
           .first_event_lower_than_threshold = 0,
           .first_event_detected = 1,
           .norm = 1,
           .average = 0,
           .mean_absolute_deviation = 2.0,
           .p_1 = first_pressure_Pa,
           .t_1 = relative_time_ms / 1000.f};
}

void ens220_setup(class esphome::ens220::ens220 *esphomeParent) {
  ESP_LOGI(esphome::ens220::TAG, "Starting ENS220 example 05_Event_Detection");

  // Start the communication, confirm the device PART_ID, and read the device UID
  ens220.begin(esphomeParent, I2C_ADDRESS);

  // while(ens220.init() != true)
  //   {
  //     Serial.println("Waiting for I2C to start");
  //     delay(1000);
  //   }

  ESP_LOGI(esphome::ens220::TAG, "Device UID: 0x%X", ens220.getUID());

  // Choose the desired configuration of the sensor. In this example we will use the settings described in the
  // application note
  ens220.setDefaultConfiguration();
  // Set the Pressure ADC conversion time (MEAS_CFG register, field P_CONV)
  ens220.setPressureConversionTime(ENS220_PRESSURE_CONVERSION_TIME_T_8_2);
  // Set the Oversampling of pressure measurements (OVS_CFG register, field OVSP)
  ens220.setOversamplingOfPressure(ENS220_OVERSAMPLING_N_32);
  // Set the Oversampling of temperature measurements (OVS_CFG register, field OVST)
  ens220.setOversamplingOfTemperature(ENS220_OVERSAMPLING_N_4);
  // Set the ratio between P and T measurements as produced by the measurement engine (MEAS_CFG register, field PT_RATE)
  ens220.setPressureTemperatureRatio(ENS220_PRESSURE_TEMPERATURE_RATIO_PT_4);
  // Set the operation to One shot (STBY_CFG register, field STBY_T)
  ens220.setStandbyTime(ENS220_STANDBY_TIME_CONTINOUS_OPERATION);
  // Set whether to use the FIFO buffer, a moving average, or none (MODE_CFG register, field FIFO_MODE)
  ens220.setPressureDataPath(ENS220_PRESSURE_DATA_PATH_DIRECT);

  // Write the desired configuration into the sensor
  ens220.writeConfiguration();

  // Start continous
  ens220.startContinuousMeasure(ENS220_SENSOR_PRESSURE);
}

void loop() {
  // Check the DATA_STAT from the sensor. Read data, if available
  relative_time_ms = esphome::millis();
  auto result = ens220.update();

  if (result == RESULT_OK) {
    // Get the values that were collected during the ens220.update()
    absolute_pressure_Pa = ens220.getPressurePascal();
    relative_pressure_Pa = absolute_pressure_Pa - first_pressure_Pa;

    // Feed the algorithm with the new values
    detect_events(absolute_pressure_Pa, relative_time_ms / 1000.f, event_threshold_multiplier, minimum_event_duration_s,
                  average_calculation_decay_rate);

    // Print all the current values (they can be plotted with the Serial Plotter)
    ESP_LOGD(esphome::ens220::TAG, "Abs_P[hPa]: %f\tRel_P[Pa]: %f\tEvent_detected: %d", absolute_pressure_Pa,
             relative_pressure_Pa, state.event_detected);

    // Light up LED if event was detected (if connected)
    toggle_led();

    last_readout_time_ms = relative_time_ms;

  } else {
    ESP_LOGW(esphome::ens220::TAG, "Read-out skipped. No data available");
  }
}

void toggle_led() {
  if (state.event_detected == 1) {
    turn_led_on();
  } else {
    turn_led_off();
  }
}

void turn_led_on() {
#if defined(OPTIONAL_LED_PIN)
  if (ledState == LOW) {
    digitalWrite(OPTIONAL_LED_PIN, HIGH);
    ledState = HIGH;
  }
#endif
}

void turn_led_off() {
#if defined(OPTIONAL_LED_PIN)
  if (ledState == HIGH) {
    digitalWrite(OPTIONAL_LED_PIN, LOW);
    ledState = LOW;
  }
#endif
}

}  // namespace ens220Driver

namespace esphome {
namespace ens220 {

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

void ens220::setup() { ens220Driver::setup(this); }

void ens220::update() {
  ens220Driver::loop();

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
