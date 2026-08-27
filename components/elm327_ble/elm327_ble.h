#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/switch/switch.h"

#include <string>
#include <vector>
#include <map>
#include <functional>

namespace esphome {
namespace elm327_ble {

// Vordefinierte OBD2 PID-Konfigurationen
struct OBD2PIDConfig {
  uint8_t mode;
  uint16_t pid;             // uint16_t fuer erweiterte PIDs (Mode 22)
  std::string command;      // z.B. "0105\r", "2201019\r" oder "ATRV\r"
  bool is_at_command;       // true fuer AT-Befehle wie ATRV
};

// Ein registrierter PID-Sensor (numerisch)
struct PIDSensorEntry {
  sensor::Sensor *sensor;
  OBD2PIDConfig config;
};

// Ein registrierter Raw-PID Text-Sensor (Hex-String Response)
struct RawPIDTextSensorEntry {
  text_sensor::TextSensor *sensor;
  uint8_t mode;
  uint16_t pid;
  std::string header;       // ECU-Header z.B. "7E4", leer = kein ATSH
  std::string command;      // z.B. "2201019\r"
  std::string expected_prefix; // z.B. "6201019" (Response-Prefix zum Zuordnen)
};

// Forward-Deklaration
class ELM327BLEHub;

// Switch zum Trennen/Verbinden der BLE-Verbindung
class ELM327BLESwitch : public switch_::Switch {
 public:
  void set_hub(ELM327BLEHub *hub) { this->hub_ = hub; }

 protected:
  void write_state(bool state) override;
  ELM327BLEHub *hub_{nullptr};
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
  void register_pid_sensor(sensor::Sensor *sensor, uint8_t mode, uint16_t pid);
  void register_at_sensor(sensor::Sensor *sensor, const std::string &command);
  void register_dtc_text_sensor(text_sensor::TextSensor *sensor);
  void register_raw_text_sensor(text_sensor::TextSensor *sensor);
  void register_raw_pid_text_sensor(text_sensor::TextSensor *sensor, uint8_t mode,
                                     uint16_t pid, const std::string &header,
                                     const std::string &command);
  void register_connected_binary_sensor(binary_sensor::BinarySensor *sensor);
  void register_engine_running_binary_sensor(binary_sensor::BinarySensor *sensor);
  void register_connection_switch(switch_::Switch *sw);

  // Oeffentlicher Befehl (fuer Service-Calls aus HA)
  void send_custom_command(const std::string &cmd);

  // BLE-Verbindung steuern (fuer Switch)
  void set_ble_enabled(bool enabled);

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
    STATE_SWITCHING_HEADER,   // Warten auf ATSH/ATST OK
    STATE_REQUESTING,
    STATE_WAITING_RESPONSE,
  };
  State state_{STATE_IDLE};

  // Initialisierung
  int init_step_{0};
  static const int INIT_STEPS_COUNT = 9;
  uint32_t last_init_time_{0};

  // PID-Abfragezyklus (numerische Sensoren)
  std::vector<PIDSensorEntry> pid_sensors_;

  // Raw-PID Text-Sensoren
  std::vector<RawPIDTextSensorEntry> raw_pid_text_sensors_;

  // Polling-Index: laeuft ueber pid_sensors_ + raw_pid_text_sensors_ + DTC
  int current_poll_index_{0};
  int total_poll_count_{0};

  uint32_t request_interval_{2000};
  uint32_t request_timeout_{10000};  // 10s fuer Multi-Frame ISO-TP Responses
  uint32_t last_request_time_{0};
  bool waiting_for_response_{false};

  // ATSH Header-Tracking
  std::string current_header_;         // Aktuell gesetzter ECU-Header
  std::string pending_header_;         // Header der noch gesetzt werden muss
  int header_switch_step_{0};          // 0=ATSH senden, 1=warten, 2=fertig
  uint32_t header_switch_time_{0};

  // Antwort-Puffer
  std::string response_buffer_;

  // Text-Sensoren
  text_sensor::TextSensor *dtc_text_sensor_{nullptr};
  text_sensor::TextSensor *raw_text_sensor_{nullptr};

  // Binary-Sensoren
  binary_sensor::BinarySensor *connected_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *engine_running_binary_sensor_{nullptr};

  // Switch
  switch_::Switch *connection_switch_{nullptr};

  // Methoden
  void send_command(const std::string &cmd);
  void run_init_sequence();
  void request_next_pid();
  void process_response(const std::string &response);
  void parse_obd2_response(const std::string &clean);
  void parse_dtc_response(const std::string &clean);
  void parse_voltage_response(const std::string &clean);
  bool dispatch_raw_pid_response(const std::string &clean);
  std::string decode_dtc(const std::string &raw);

  // Sensor-Lookup
  sensor::Sensor *find_sensor_for_pid(uint8_t pid);
  sensor::Sensor *find_at_sensor(const std::string &command);

  // Hilfsfunktionen
  std::string get_header_for_poll_index(int idx);
  std::string get_command_for_poll_index(int idx);
  void update_total_poll_count();
};

}  // namespace elm327_ble
}  // namespace esphome
