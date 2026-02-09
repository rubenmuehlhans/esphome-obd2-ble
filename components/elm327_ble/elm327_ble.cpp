#include "elm327_ble.h"
#include "esphome/core/log.h"

namespace espbt = esphome::esp32_ble_tracker;

namespace esphome {
namespace elm327_ble {

static const char *TAG = "elm327_ble";

void ELM327BLEHub::setup() {
  ESP_LOGCONFIG(TAG, "ELM327 BLE Hub wird initialisiert...");
}

void ELM327BLEHub::dump_config() {
  ESP_LOGCONFIG(TAG, "ELM327 BLE Hub:");
  ESP_LOGCONFIG(TAG, "  Service UUID: %s", this->service_uuid_str_.c_str());
  ESP_LOGCONFIG(TAG, "  TX Char UUID: %s", this->char_tx_uuid_str_.c_str());
  ESP_LOGCONFIG(TAG, "  RX Char UUID: %s", this->char_rx_uuid_str_.c_str());
  ESP_LOGCONFIG(TAG, "  Abfrageintervall: %u ms", this->request_interval_);
  ESP_LOGCONFIG(TAG, "  Timeout: %u ms", this->request_timeout_);
  ESP_LOGCONFIG(TAG, "  Registrierte PID-Sensoren: %d", (int) this->pid_sensors_.size());
  if (this->dtc_text_sensor_ != nullptr)
    ESP_LOGCONFIG(TAG, "  DTC Text Sensor: ja");
}

// ============================================================
// BLE GATTC Event Handler
// ============================================================
void ELM327BLEHub::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                         esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_OPEN_EVT: {
      if (param->open.status == ESP_GATT_OK) {
        ESP_LOGI(TAG, "BLE: Verbunden mit ELM327");
        this->state_ = STATE_CONNECTED;
        if (this->connected_binary_sensor_ != nullptr)
          this->connected_binary_sensor_->publish_state(false);  // noch nicht initialisiert
      }
      break;
    }

    case ESP_GATTC_DISCONNECT_EVT: {
      ESP_LOGW(TAG, "BLE: ELM327 getrennt!");
      this->state_ = STATE_IDLE;
      this->handles_resolved_ = false;
      this->init_step_ = 0;
      this->waiting_for_response_ = false;
      this->response_buffer_.clear();
      if (this->connected_binary_sensor_ != nullptr)
        this->connected_binary_sensor_->publish_state(false);
      break;
    }

    case ESP_GATTC_SEARCH_CMPL_EVT: {
      // Service Discovery abgeschlossen, Handles suchen
      auto *chr_tx = this->parent()->get_characteristic(
          espbt::ESPBTUUID::from_raw(this->service_uuid_str_),
          espbt::ESPBTUUID::from_raw(this->char_tx_uuid_str_));
      auto *chr_rx = this->parent()->get_characteristic(
          espbt::ESPBTUUID::from_raw(this->service_uuid_str_),
          espbt::ESPBTUUID::from_raw(this->char_rx_uuid_str_));

      if (chr_tx == nullptr || chr_rx == nullptr) {
        ESP_LOGE(TAG, "BLE: Characteristics nicht gefunden!");
        break;
      }

      this->char_tx_handle_ = chr_tx->handle;
      this->char_rx_handle_ = chr_rx->handle;
      this->handles_resolved_ = true;

      ESP_LOGI(TAG, "BLE: TX Handle=0x%04X, RX Handle=0x%04X",
               this->char_tx_handle_, this->char_rx_handle_);

      // Notify registrieren für RX Characteristic
      auto status = esp_ble_gattc_register_for_notify(
          this->parent()->get_gattc_if(), this->parent()->get_remote_bda(), this->char_rx_handle_);
      if (status != ESP_OK) {
        ESP_LOGW(TAG, "BLE: Notify-Registrierung fehlgeschlagen: %d", status);
      }
      break;
    }

    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      ESP_LOGI(TAG, "BLE: Notify registriert, starte Initialisierung...");
      this->state_ = STATE_INITIALIZING;
      this->init_step_ = 0;
      this->last_init_time_ = 0;
      break;
    }

    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.handle != this->char_rx_handle_)
        break;

      // Daten zum Puffer hinzufügen
      std::string chunk((char *) param->notify.value, param->notify.value_len);
      ESP_LOGV(TAG, "Empfangen (raw): %s", chunk.c_str());
      this->response_buffer_ += chunk;

      // Prüfe ob Antwort komplett (ELM327 sendet '>' als Prompt)
      if (this->response_buffer_.find('>') != std::string::npos) {
        std::string full = this->response_buffer_;
        this->response_buffer_.clear();
        this->process_response(full);
      }
      break;
    }

    default:
      break;
  }
}

// ============================================================
// Main Loop
// ============================================================
void ELM327BLEHub::loop() {
  uint32_t now = millis();

  switch (this->state_) {
    case STATE_INITIALIZING:
      this->run_init_sequence();
      break;

    case STATE_READY:
      // Nächste PID-Abfrage senden
      if (!this->waiting_for_response_ && (now - this->last_request_time_ >= this->request_interval_)) {
        this->request_next_pid();
      }
      // Timeout prüfen
      if (this->waiting_for_response_ && (now - this->last_request_time_ >= this->request_timeout_)) {
        ESP_LOGW(TAG, "Antwort-Timeout, mache weiter...");
        this->waiting_for_response_ = false;
        this->response_buffer_.clear();
      }
      break;

    default:
      break;
  }
}

// ============================================================
// ELM327 Initialisierung
// ============================================================
void ELM327BLEHub::run_init_sequence() {
  uint32_t now = millis();

  struct InitCmd {
    const char *cmd;
    uint32_t delay_after;
    const char *description;
  };

  static const InitCmd init_cmds[] = {
    {"ATZ\r",   2000, "Reset"},
    {"ATE0\r",   500, "Echo aus"},
    {"ATL0\r",   500, "Linefeed aus"},
    {"ATS0\r",   500, "Spaces aus"},
    {"ATH0\r",   500, "Headers aus"},
    {"ATSP0\r", 1000, "Auto-Protokoll"},
    {"0100\r",  5000, "Protokoll-Erkennung"},
  };

  if (this->init_step_ >= INIT_STEPS_COUNT) {
    // Initialisierung abgeschlossen
    ESP_LOGI(TAG, "ELM327 initialisiert - bereit fuer Abfragen");
    this->state_ = STATE_READY;
    this->current_pid_index_ = 0;
    this->waiting_for_response_ = false;
    if (this->connected_binary_sensor_ != nullptr)
      this->connected_binary_sensor_->publish_state(true);
    return;
  }

  if (this->last_init_time_ == 0 || (now - this->last_init_time_ >= init_cmds[this->init_step_].delay_after)) {
    if (this->last_init_time_ != 0) {
      // Nächster Schritt
      this->init_step_++;
      if (this->init_step_ >= INIT_STEPS_COUNT) {
        // Letzter Delay abgelaufen, fertig
        this->run_init_sequence();
        return;
      }
    }

    ESP_LOGD(TAG, "Init [%d/%d]: %s", this->init_step_ + 1, INIT_STEPS_COUNT,
             init_cmds[this->init_step_].description);
    this->send_command(init_cmds[this->init_step_].cmd);
    this->last_init_time_ = now;
  }
}

// ============================================================
// BLE Write
// ============================================================
void ELM327BLEHub::send_command(const std::string &cmd) {
  if (!this->handles_resolved_) {
    ESP_LOGW(TAG, "Kann nicht senden - BLE Handles nicht aufgeloest");
    return;
  }

  auto status = esp_ble_gattc_write_char(
      this->parent()->get_gattc_if(),
      this->parent()->get_conn_id(),
      this->char_tx_handle_,
      cmd.size(),
      (uint8_t *) cmd.data(),
      ESP_GATT_WRITE_TYPE_RSP,
      ESP_GATT_AUTH_REQ_NONE);

  if (status != ESP_OK) {
    ESP_LOGW(TAG, "BLE Write fehlgeschlagen: %d", status);
  }
}

// ============================================================
// PID-Abfrage-Zyklus
// ============================================================
void ELM327BLEHub::request_next_pid() {
  if (this->pid_sensors_.empty() && this->dtc_text_sensor_ == nullptr)
    return;

  // Gesamtliste: PID-Sensoren + optional DTC
  int total = this->pid_sensors_.size();
  bool has_dtc = (this->dtc_text_sensor_ != nullptr);
  if (has_dtc) total++;

  if (total == 0) return;

  int idx = this->current_pid_index_ % total;

  std::string cmd;
  if (idx < (int) this->pid_sensors_.size()) {
    cmd = this->pid_sensors_[idx].config.command;
    ESP_LOGD(TAG, "PID[%d/%d] gesendet: %s", idx + 1, total, cmd.c_str());
  } else {
    // DTC-Abfrage
    cmd = "03\r";
    ESP_LOGD(TAG, "DTC Abfrage [%d/%d] gesendet", idx + 1, total);
  }

  this->response_buffer_.clear();
  this->waiting_for_response_ = true;
  this->last_request_time_ = millis();
  this->send_command(cmd);
  this->current_pid_index_ = (idx + 1) % total;
}

// ============================================================
// Antwort-Verarbeitung
// ============================================================
void ELM327BLEHub::process_response(const std::string &response) {
  this->waiting_for_response_ = false;

  // Bereinigen
  std::string clean;
  for (char c : response) {
    if (c != ' ' && c != '\r' && c != '\n' && c != '>') {
      clean += c;
    }
  }

  ESP_LOGD(TAG, "Antwort: %s", clean.c_str());

  // Debug: Raw Text Sensor
  if (this->raw_text_sensor_ != nullptr) {
    this->raw_text_sensor_->publish_state(clean);
  }

  // Fehler ignorieren
  if (clean.find("NODATA") != std::string::npos ||
      clean.find("ERROR") != std::string::npos ||
      clean.find("UNABLE") != std::string::npos ||
      clean.find("STOPPED") != std::string::npos) {
    ESP_LOGW(TAG, "Fehler/Keine Daten: %s", clean.c_str());
    return;
  }

  // DTC-Antwort (Mode 03, beginnt mit "43")
  if (clean.find("43") != std::string::npos) {
    this->parse_dtc_response(clean);
    return;
  }

  // Batteriespannung (ATRV, z.B. "12.4V")
  if (clean.find("V") != std::string::npos && clean.find("41") == std::string::npos) {
    this->parse_voltage_response(clean);
    return;
  }

  // OBD2 Mode 01 Antwort (beginnt mit "41")
  if (clean.find("41") != std::string::npos) {
    this->parse_obd2_response(clean);
    return;
  }
}

// ============================================================
// OBD2 PID Parsing
// ============================================================
void ELM327BLEHub::parse_obd2_response(const std::string &clean) {
  size_t pos = clean.find("41");
  if (pos == std::string::npos) return;

  std::string data = clean.substr(pos);
  if (data.length() < 6) return;

  auto hex_byte = [](const std::string &s, int off) -> int {
    if (off + 2 > (int) s.length()) return -1;
    return (int) strtol(s.substr(off, 2).c_str(), nullptr, 16);
  };

  int pid = hex_byte(data, 2);
  int a = hex_byte(data, 4);
  int b = (data.length() >= 8) ? hex_byte(data, 6) : 0;

  if (pid < 0 || a < 0) return;

  sensor::Sensor *sensor = this->find_sensor_for_pid(pid);
  if (sensor == nullptr) {
    ESP_LOGD(TAG, "Kein Sensor fuer PID 0x%02X registriert", pid);
    return;
  }

  float value = 0;
  switch (pid) {
    case 0x04:  // Motorlast: A*100/255
      value = (a * 100.0f) / 255.0f;
      break;
    case 0x05:  // Kühlmitteltemperatur: A - 40
      value = a - 40.0f;
      break;
    case 0x0B:  // MAP: A
      value = a;
      break;
    case 0x0C:  // Drehzahl: ((A*256)+B)/4
      value = ((a * 256) + b) / 4.0f;
      break;
    case 0x0D:  // Geschwindigkeit: A
      value = a;
      break;
    case 0x0F:  // Ansauglufttemperatur: A - 40
      value = a - 40.0f;
      break;
    case 0x10:  // MAF: ((A*256)+B)/100
      value = ((a * 256) + b) / 100.0f;
      break;
    case 0x11:  // Drosselklappe: A*100/255
      value = (a * 100.0f) / 255.0f;
      break;
    case 0x1F:  // Motorlaufzeit: (A*256)+B
      value = (a * 256) + b;
      break;
    case 0x2E:  // AGR: A*100/255
      value = (a * 100.0f) / 255.0f;
      break;
    case 0x2F:  // Kraftstoffstand: A*100/255
      value = (a * 100.0f) / 255.0f;
      break;
    case 0x33:  // Barometrischer Druck: A
      value = a;
      break;
    case 0x42:  // ECU Spannung: ((A*256)+B)/1000
      value = ((a * 256) + b) / 1000.0f;
      break;
    case 0x46:  // Umgebungstemperatur: A - 40
      value = a - 40.0f;
      break;
    case 0x5C:  // Öltemperatur: A - 40
      value = a - 40.0f;
      break;
    case 0x5E:  // Kraftstoffverbrauch: ((A*256)+B)/20
      value = ((a * 256) + b) / 20.0f;
      break;
    default:
      // Generische Formel: einfach A zurückgeben
      value = a;
      ESP_LOGD(TAG, "PID 0x%02X: generisch A=%d", pid, a);
      break;
  }

  sensor->publish_state(value);
  ESP_LOGD(TAG, "PID 0x%02X = %.2f", pid, value);

  // Motor-Lauf-Status aktualisieren (basierend auf RPM)
  if (pid == 0x0C && this->engine_running_binary_sensor_ != nullptr) {
    this->engine_running_binary_sensor_->publish_state(value > 0);
  }
}

// ============================================================
// DTC Parsing
// ============================================================
void ELM327BLEHub::parse_dtc_response(const std::string &clean) {
  if (this->dtc_text_sensor_ == nullptr) return;

  size_t dpos = clean.find("43");
  if (dpos == std::string::npos) return;

  std::string dtc_data = clean.substr(dpos);
  std::string dtc_list;
  int dtc_count = 0;

  // DTCs starten ab Position 2 (nach "43"), je 4 Hex-Zeichen = 2 Bytes
  for (size_t i = 2; i + 3 < dtc_data.length(); i += 4) {
    std::string dtc_raw = dtc_data.substr(i, 4);
    if (dtc_raw == "0000") continue;

    std::string dtc_code = this->decode_dtc(dtc_raw);
    if (!dtc_list.empty()) dtc_list += ", ";
    dtc_list += dtc_code;
    dtc_count++;
  }

  if (dtc_count == 0) {
    this->dtc_text_sensor_->publish_state("Keine Fehler");
  } else {
    this->dtc_text_sensor_->publish_state(dtc_list);
  }
  ESP_LOGD(TAG, "DTCs (%d): %s", dtc_count, dtc_list.c_str());
}

std::string ELM327BLEHub::decode_dtc(const std::string &raw) {
  if (raw.length() < 4) return "????";

  char first = raw[0];
  char prefix;
  switch (first) {
    case '0': case '1': case '2': case '3': prefix = 'P'; break;
    case '4': case '5': prefix = 'C'; break;
    case '6': case '7': prefix = 'B'; break;
    case '8': case '9': case 'A': case 'B': prefix = 'U'; break;
    default: prefix = 'P'; break;
  }

  char second = '0' + (((first >= 'A' ? first - 'A' + 10 : first - '0') % 4));

  std::string result;
  result += prefix;
  result += second;
  result += raw.substr(1, 3);
  return result;
}

// ============================================================
// Batteriespannung (ATRV)
// ============================================================
void ELM327BLEHub::parse_voltage_response(const std::string &clean) {
  sensor::Sensor *sensor = this->find_at_sensor("ATRV\r");
  if (sensor == nullptr) return;

  std::string volt_str;
  for (char c : clean) {
    if ((c >= '0' && c <= '9') || c == '.') {
      volt_str += c;
    }
  }

  if (!volt_str.empty()) {
    float voltage = atof(volt_str.c_str());
    if (voltage > 0 && voltage < 20) {
      sensor->publish_state(voltage);
      ESP_LOGD(TAG, "Batterie: %.1f V", voltage);
    }
  }
}

// ============================================================
// Sensor-Registrierung
// ============================================================
void ELM327BLEHub::register_pid_sensor(sensor::Sensor *sensor, uint8_t mode, uint8_t pid) {
  PIDSensorEntry entry;
  entry.sensor = sensor;
  entry.config.mode = mode;
  entry.config.pid = pid;
  entry.config.is_at_command = false;
  // Befehl generieren: z.B. mode=0x01 pid=0x05 → "0105\r"
  char cmd[8];
  snprintf(cmd, sizeof(cmd), "%02X%02X\r", mode, pid);
  entry.config.command = cmd;
  this->pid_sensors_.push_back(entry);
  ESP_LOGD(TAG, "PID Sensor registriert: Mode 0x%02X PID 0x%02X → %s", mode, pid, cmd);
}

void ELM327BLEHub::register_at_sensor(sensor::Sensor *sensor, const std::string &command) {
  PIDSensorEntry entry;
  entry.sensor = sensor;
  entry.config.mode = 0;
  entry.config.pid = 0;
  entry.config.is_at_command = true;
  entry.config.command = command;
  this->pid_sensors_.push_back(entry);
  ESP_LOGD(TAG, "AT Sensor registriert: %s", command.c_str());
}

void ELM327BLEHub::register_dtc_text_sensor(text_sensor::TextSensor *sensor) {
  this->dtc_text_sensor_ = sensor;
}

void ELM327BLEHub::register_raw_text_sensor(text_sensor::TextSensor *sensor) {
  this->raw_text_sensor_ = sensor;
}

void ELM327BLEHub::register_connected_binary_sensor(binary_sensor::BinarySensor *sensor) {
  this->connected_binary_sensor_ = sensor;
}

void ELM327BLEHub::register_engine_running_binary_sensor(binary_sensor::BinarySensor *sensor) {
  this->engine_running_binary_sensor_ = sensor;
}

// ============================================================
// Sensor Lookup
// ============================================================
sensor::Sensor *ELM327BLEHub::find_sensor_for_pid(uint8_t pid) {
  for (auto &entry : this->pid_sensors_) {
    if (!entry.config.is_at_command && entry.config.pid == pid) {
      return entry.sensor;
    }
  }
  return nullptr;
}

sensor::Sensor *ELM327BLEHub::find_at_sensor(const std::string &command) {
  for (auto &entry : this->pid_sensors_) {
    if (entry.config.is_at_command && entry.config.command == command) {
      return entry.sensor;
    }
  }
  return nullptr;
}

}  // namespace elm327_ble
}  // namespace esphome
