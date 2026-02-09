#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

#include <string>
#include <vector>
#include <map>
#include <functional>

namespace esphome {
namespace elm327_ble {

// Vordefinierte OBD2 PID-Konfigurationen
struct OBD2PIDConfig {
  uint8_t mode;
  uint8_t pid;
  std::string command;  // z.B. "0105\r" oder "ATRV\r"
  bool is_at_command;   // true für AT-Befehle wie ATRV
};

// Ein registrierter PID-Sensor
struct PIDSensorEntry {
  sensor::Sensor *sensor;
  OBD2PIDConfig config;
};

class ELM327BLEHub : public Component, public ble_client::BLEClientNode {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_BLUETOOTH; }

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

  // Setters (aufgerufen vom Python-Codegen)
  void set_service_uuid(const std::string &uuid) { this->service_uuid_str_ = uuid; }
  void set_char_tx_uuid(const std::string &uuid) { this->char_tx_uuid_str_ = uuid; }
  void set_char_rx_uuid(const std::string &uuid) { this->char_rx_uuid_str_ = uuid; }
  void set_request_interval(uint32_t interval_ms) { this->request_interval_ = interval_ms; }
  void set_request_timeout(uint32_t timeout_ms) { this->request_timeout_ = timeout_ms; }

  // Sensoren registrieren
  void register_pid_sensor(sensor::Sensor *sensor, uint8_t mode, uint8_t pid);
  void register_at_sensor(sensor::Sensor *sensor, const std::string &command);
  void register_dtc_text_sensor(text_sensor::TextSensor *sensor);
  void register_raw_text_sensor(text_sensor::TextSensor *sensor);
  void register_connected_binary_sensor(binary_sensor::BinarySensor *sensor);
  void register_engine_running_binary_sensor(binary_sensor::BinarySensor *sensor);

 protected:
  // BLE UUIDs
  std::string service_uuid_str_;
  std::string char_tx_uuid_str_;
  std::string char_rx_uuid_str_;

  // BLE Handles
  uint16_t char_tx_handle_{0};
  uint16_t char_rx_handle_{0};
  bool handles_resolved_{false};

  // ELM327 State Machine
  enum State {
    STATE_IDLE,
    STATE_CONNECTED,
    STATE_INITIALIZING,
    STATE_READY,
    STATE_REQUESTING,
    STATE_WAITING_RESPONSE,
  };
  State state_{STATE_IDLE};

  // Initialisierung
  int init_step_{0};
  static const int INIT_STEPS_COUNT = 7;
  uint32_t last_init_time_{0};

  // PID-Abfragezyklus
  std::vector<PIDSensorEntry> pid_sensors_;
  int current_pid_index_{0};
  uint32_t request_interval_{2000};
  uint32_t request_timeout_{5000};
  uint32_t last_request_time_{0};
  bool waiting_for_response_{false};

  // Antwort-Puffer
  std::string response_buffer_;

  // Text-Sensoren
  text_sensor::TextSensor *dtc_text_sensor_{nullptr};
  text_sensor::TextSensor *raw_text_sensor_{nullptr};

  // Binary-Sensoren
  binary_sensor::BinarySensor *connected_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *engine_running_binary_sensor_{nullptr};

  // Methoden
  void send_command(const std::string &cmd);
  void run_init_sequence();
  void request_next_pid();
  void process_response(const std::string &response);
  void parse_obd2_response(const std::string &clean);
  void parse_dtc_response(const std::string &clean);
  void parse_voltage_response(const std::string &clean);
  std::string decode_dtc(const std::string &raw);

  // Sensor-Lookup
  sensor::Sensor *find_sensor_for_pid(uint8_t pid);
  sensor::Sensor *find_at_sensor(const std::string &command);
};

}  // namespace elm327_ble
}  // namespace esphome
