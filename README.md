# ESPHome OBD2 BLE - ELM327 Custom Component

![ESPHome CI](https://github.com/rubenmuehlhans/esphome-obd2-ble/actions/workflows/ci.yml/badge.svg)

ESPHome Custom Component zum Auslesen von OBD2-Fahrzeugdaten per BLE (Bluetooth Low Energy) mit ELM327-kompatiblen Dongles. 17 Live-Sensoren, Fehlerspeicher-Auslesen, Raw-PID Text-Sensoren für E-Fahrzeuge (Mode 22) und fertiges Home Assistant Dashboard.

Getestet mit **Fiat Ducato 250** + **Veepeak OBDCheck BLE+** + **M5Stack ATOM S3**.

---

## Inhaltsverzeichnis

- [Voraussetzungen](#voraussetzungen)
- [Hardware](#hardware)
- [Zwei Varianten](#zwei-varianten)
- [Schnellstart](#schnellstart-custom-component)
- [Verfügbare Sensor-Typen](#verfügbare-sensor-typen)
- [Raw PID Text-Sensoren (Mode 22 / EV)](#raw-pid-text-sensoren-mode-22--ev)
- [ECU Header (ATSH)](#ecu-header-atsh)
- [Service-Call von Home Assistant](#service-call-von-home-assistant)
- [Kia Niro EV Beispiel](#kia-niro-ev-beispiel)
- [Hub-Konfiguration](#hub-konfiguration)
- [Home Assistant Dashboard](#home-assistant-dashboard)
- [Sicherheit](#sicherheit)
- [Troubleshooting](#troubleshooting)
- [Optimierung](#optimierung)
- [Kompatibilität](#kompatibilität)

---

## Voraussetzungen

- **Home Assistant** mit installiertem **ESPHome Add-on** (oder ESPHome CLI)
- **ESP32** Mikrocontroller (z.B. M5Stack ATOM S3, ESP32-DevKit, etc.)
- **ELM327 BLE Dongle** im OBD2-Port des Fahrzeugs
- Fahrzeug mit **OBD2-Anschluss** (Benziner ab 2001, Diesel ab 2004)
- **nRF Connect App** (optional, zum Ermitteln der BLE UUIDs)

## Hardware

| Komponente | Beschreibung |
|---|---|
| **ESP32** (z.B. M5Stack ATOM S3) | Mikrocontroller mit BLE-Unterstützung |
| **ELM327 BLE Dongle** (z.B. Veepeak OBDCheck BLE+) | OBD2-zu-BLE Adapter |
| **Fahrzeug mit OBD2** | Ab Baujahr 2001 (Benzin) / 2004 (Diesel) |

### Aufbau

```
OBD2-Port ←→ ELM327 BLE Dongle ←·····BLE·····→ ESP32 ←···WiFi···→ Home Assistant
```

## Zwei Varianten

Dieses Repo enthält zwei Wege um OBD2-Daten auszulesen:

### 1. Custom Component (empfohlen)

Saubere ESPHome-Komponente mit eigenem YAML-Schema. Einfach zu konfigurieren, leicht erweiterbar.

```yaml
# Sensor hinzufügen = 3 Zeilen:
sensor:
  - platform: elm327_ble
    type: rpm
    name: "Drehzahl"
```

Dateien: `components/elm327_ble/`, `example-component.yaml`

### 2. Standalone YAML (alles in einer Datei)

Klassischer Ansatz mit Inline-Lambdas. Funktioniert ohne External Components, ist aber schwerer anzupassen.

Datei: `ducato-obd2-atom-s3.yaml`

## Dateien

| Datei / Ordner | Beschreibung |
|---|---|
| `components/elm327_ble/` | Custom Component (C++ & Python) |
| `example-component.yaml` | Beispiel-Config: Ducato (Mode 01 PIDs) |
| `example-kia-niro-ev.yaml` | Beispiel-Config: Kia Niro EV (Mode 22 Raw PIDs) |
| `ducato-obd2-atom-s3.yaml` | Standalone-Config (ohne Component) |
| `ducato-ble-scanner.yaml` | BLE-Scanner (Phase 1: Geräte finden, Phase 2: Characteristics auslesen) |
| `dashboard-ducato-obd2.yaml` | Fertiges Home Assistant Dashboard |
| `secrets.yaml.example` | Vorlage für secrets.yaml |

---

## Schnellstart (Custom Component)

### Schritt 1: BLE-Daten ermitteln (einmalig)

Bevor du loslegst, musst du die **MAC-Adresse** und die **BLE UUIDs** deines ELM327-Dongles herausfinden. Jeder Dongle hat andere Werte.

**Option A: BLE-Scanner flashen (empfohlen)**

Der BLE-Scanner (`ducato-ble-scanner.yaml`) arbeitet in zwei Phasen:

**Phase 1 - Gerät finden:**
1. `ducato-ble-scanner.yaml` in ESPHome einfügen und auf den ESP32 flashen
2. ELM327-Dongle in den OBD2-Port stecken, Zündung einschalten
3. ESPHome-Logs beobachten — nach dem Namen deines Dongles suchen (z.B. `VEEPEAK`, `IOS-Vlink`, `OBDII`)
4. **MAC-Adresse** notieren

**Phase 2 - Characteristics auslesen:**
1. Die notierte MAC-Adresse im `ble_client`-Block eintragen (Kommentarzeichen entfernen)
2. Erneut flashen
3. Im Log erscheinen alle GATT Services und Characteristics mit Properties:
   - Characteristic mit **NOTIFY** = RX (Empfang, `char_rx_uuid`)
   - Characteristic mit **WRITE** = TX (Senden, `char_tx_uuid`)
4. Service-UUID, TX-UUID und RX-UUID notieren

**Option B: nRF Connect App (Handy)**

1. App [nRF Connect](https://www.nordicsemi.com/Products/Development-tools/nrf-connect-for-mobile) installieren (iOS/Android)
2. Dongle in OBD2-Port, Zündung an
3. In der App scannen, Dongle finden, verbinden
4. **MAC-Adresse**, **Service UUID** und **Characteristics** notieren

**Typische Veepeak UUIDs:**

| Variante | Service | RX (Notify) | TX (Write) |
|---|---|---|---|
| A | `FFE0` | `FFE1` | `FFE1` |
| B | `FFF0` | `FFF1` | `FFF2` |

> **Wichtig:** Die UUIDs variieren je nach Dongle-Modell und Firmware-Version. Immer selbst prüfen!

### Schritt 2: secrets.yaml anlegen

```bash
cp secrets.yaml.example secrets.yaml
```

Dann die Datei öffnen und deine Werte eintragen:

```yaml
wifi_ssid: "DeinWLAN"
wifi_password: "DeinPasswort"
ap_password: "FallbackPasswort"
api_key: "dein-base64-key"      # Generieren mit: esphome gen-key
ota_password: "dein-ota-passwort"
```

### Schritt 3: Config erstellen

Erstelle eine neue YAML-Datei in ESPHome (oder kopiere `example-component.yaml`) und passe die markierten Stellen an:

```yaml
esphome:
  name: mein-auto-obd2
  friendly_name: "Mein Auto OBD2"

esp32:
  board: m5stack-atoms3       # ← Dein ESP32-Board anpassen!
  variant: esp32s3
  framework:
    type: arduino

logger:
api:
  encryption:
    key: !secret api_key
ota:
  - platform: esphome
    password: !secret ota_password
wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password
  fast_connect: true
  ap:
    ssid: "OBD2-Fallback"
    password: !secret ap_password

# --- Custom Component laden ---
external_components:
  - source: github://rubenmuehlhans/esphome-obd2-ble
    components: [elm327_ble]

# --- BLE Verbindung ---
esp32_ble_tracker:
  scan_parameters:
    active: true

ble_client:
  - mac_address: "AA:BB:CC:DD:EE:FF"  # ← Deine MAC-Adresse!
    id: elm327_ble_client

# --- ELM327 Hub ---
elm327_ble:
  id: elm327_hub
  ble_client_id: elm327_ble_client
  service_uuid: "0000FFF0-0000-1000-8000-00805F9B34FB"  # ← Deine UUIDs!
  char_tx_uuid: "0000FFF2-0000-1000-8000-00805F9B34FB"
  char_rx_uuid: "0000FFF1-0000-1000-8000-00805F9B34FB"

# --- Sensoren (nur die gewünschten eintragen) ---
sensor:
  - platform: elm327_ble
    type: rpm
    name: "Drehzahl"

  - platform: elm327_ble
    type: speed
    name: "Geschwindigkeit"

  - platform: elm327_ble
    type: coolant_temp
    name: "Motortemperatur"

  - platform: elm327_ble
    type: battery_voltage
    name: "Batteriespannung"

# --- Fehlerspeicher ---
text_sensor:
  - platform: elm327_ble
    type: dtc
    name: "Fehlercodes"

# --- Status ---
binary_sensor:
  - platform: elm327_ble
    type: engine_running
    name: "Motor läuft"

  - platform: elm327_ble
    type: connected
    name: "ELM327 verbunden"
```

> **Tipp:** Starte mit wenigen Sensoren und füge nach und nach weitere hinzu. Nicht jedes Fahrzeug unterstützt alle PIDs.

### Schritt 4: Flashen

1. In Home Assistant → ESPHome Dashboard → YAML einfügen
2. **Install** klicken
3. Erstes Mal per **USB** flashen
4. Danach sind OTA-Updates per WiFi möglich

---

## Verfügbare Sensor-Typen

### sensor (platform: elm327_ble)

| type | Name | PID | Einheit | Formel |
|---|---|---|---|---|
| `coolant_temp` | Motortemperatur | `0x05` | °C | A - 40 |
| `rpm` | Drehzahl | `0x0C` | RPM | ((A*256)+B) / 4 |
| `speed` | Geschwindigkeit | `0x0D` | km/h | A |
| `engine_load` | Motorlast | `0x04` | % | A*100 / 255 |
| `intake_temp` | Ansauglufttemperatur | `0x0F` | °C | A - 40 |
| `fuel_level` | Kraftstoffstand | `0x2F` | % | A*100 / 255 |
| `throttle` | Drosselklappe | `0x11` | % | A*100 / 255 |
| `battery_voltage` | Batteriespannung | `ATRV` | V | direkt |
| `intake_map` | Ansaugkrümmerdruck | `0x0B` | kPa | A |
| `maf` | Luftmassenmesser | `0x10` | g/s | ((A*256)+B) / 100 |
| `engine_runtime` | Motorlaufzeit | `0x1F` | s | (A*256)+B |
| `oil_temp` | Motoröltemperatur | `0x5C` | °C | A - 40 |
| `ambient_temp` | Umgebungstemperatur | `0x46` | °C | A - 40 |
| `ecu_voltage` | ECU Spannung | `0x42` | V | ((A*256)+B) / 1000 |
| `fuel_rate` | Kraftstoffverbrauch | `0x5E` | L/h | ((A*256)+B) / 20 |
| `baro_pressure` | Barometrischer Druck | `0x33` | kPa | A |
| `egr` | Abgasrückführung | `0x2E` | % | A*100 / 255 |

### Eigene PIDs abfragen

Du kannst auch PIDs abfragen, die nicht in der Liste oben stehen. In dem Fall wird der Rohwert des ersten Datenbytes (A) zurückgegeben. PIDs können bis zu 16 Bit lang sein (für Mode 22).

```yaml
sensor:
  - platform: elm327_ble
    name: "Mein Custom PID"
    mode: 0x01
    pid: 0x21
    unit_of_measurement: "Schritte"
    accuracy_decimals: 0
```

### text_sensor (platform: elm327_ble)

| type | Beschreibung |
|---|---|
| `dtc` | Aktive Fehlercodes (z.B. `P0123, P0456` oder `Keine Fehler`) |
| `raw` | Debug: letzte Rohantwort vom ELM327 |
| `raw_pid` | **Neu:** Rohe Hex-Response einer bestimmten PID als Text-Sensor |

### binary_sensor (platform: elm327_ble)

| type | Beschreibung |
|---|---|
| `connected` | ELM327 BLE-Verbindung hergestellt und initialisiert |
| `engine_running` | Motor läuft (Drehzahl > 0) |

---

## Raw PID Text-Sensoren (Mode 22 / EV)

Für Elektrofahrzeuge und andere Fahrzeuge mit erweiterten Diagnose-PIDs (Mode 22) gibt es den neuen Text-Sensor-Typ `raw_pid`. Statt die Daten auf dem ESP32 zu parsen, wird die **rohe Hex-Antwort** als Text-Sensor an Home Assistant übertragen. Die Berechnung der einzelnen Werte erfolgt dann in HA per Template-Sensor.

**Vorteile:**
- Kein Firmware-Build nötig wenn sich Formeln ändern
- Beliebig viele Werte aus einer PID-Response extrahierbar
- Funktioniert mit jedem Fahrzeug-Profil (WiCAN, Torque, etc.)
- Flexible Anpassung in Home Assistant

### Konfiguration

```yaml
text_sensor:
  # Mit Mode + PID (empfohlen):
  - platform: elm327_ble
    type: raw_pid
    name: "BMS Daten Raw"
    mode: 0x22            # Mode 22 = Enhanced Diagnostic
    pid: 0x01019          # PID (bis zu 16 Bit)
    header: "7E4"         # Optional: ECU-Adresse (ATSH)

  # Mit beliebigem Command:
  - platform: elm327_ble
    type: raw_pid
    name: "Custom Command Raw"
    command: "2201019\r"  # Direkter OBD2-Befehl
    header: "7E4"
```

### Parameter

| Parameter | Pflicht | Beschreibung |
|---|---|---|
| `type` | ja | `raw_pid` |
| `mode` | nein* | OBD2 Mode (Default: `0x22`) |
| `pid` | nein* | PID-Nummer (hex, bis 16 Bit) |
| `command` | nein* | Alternativer direkter Befehl-String |
| `header` | nein | ECU-Adresse für ATSH (z.B. `"7E4"`) |

*Entweder `pid` (mit optionalem `mode`) oder `command` muss angegeben werden.

### Response-Format

Die Response wird als bereinigter Hex-String publiziert (ohne Leerzeichen, `\r`, `\n`, `>`). Beispiel:

```
Befehl:  2201019
Response: 6201019...AABBCCDD...
```

Der Hex-String beginnt mit dem Response-Prefix (`62` = Mode 22 Response) gefolgt von der PID und den Datenbytes.

---

## ECU Header (ATSH)

Viele Fahrzeuge (besonders EVs) haben mehrere Steuergeräte (ECUs), die über unterschiedliche CAN-IDs angesprochen werden. Mit dem `header`-Parameter wird vor der PID-Abfrage automatisch `ATSH{header}` gesendet.

```yaml
text_sensor:
  # BMS auf ECU 7E4
  - platform: elm327_ble
    type: raw_pid
    name: "BMS Daten"
    pid: 0x01019
    header: "7E4"

  # Cluster auf ECU 7C6
  - platform: elm327_ble
    type: raw_pid
    name: "Cluster Daten"
    pid: 0xB002
    header: "7C6"
```

Der Header-Wechsel erfolgt automatisch: Wenn der nächste Sensor einen anderen Header braucht als aktuell gesetzt, wird zuerst `ATSH{neuer_header}` gesendet und nach 500ms die PID abgefragt. Sensoren ohne `header` verwenden den Standard-Broadcast.

**Typische ECU-Adressen (Kia/Hyundai EV):**

| ECU | Adresse | Beschreibung |
|---|---|---|
| BMS | `7E4` | Battery Management System |
| MCU | `7E2` | Motor Control Unit |
| OBC | `7E5` | On-Board Charger |
| Cluster | `7C6` | Instrumententafel |
| IGMP | `770` | Integrated Gateway Module |

---

## Service-Call von Home Assistant

Du kannst beliebige ELM327-Befehle aus Home Assistant an den ESP32 senden. Dazu einen API-Service in der ESPHome-Config definieren:

```yaml
api:
  services:
    - service: send_elm327_command
      variables:
        command: string
      then:
        - lambda: |-
            id(elm327_hub).send_custom_command(command + "\r");
```

**Aufruf in Home Assistant:**

```yaml
service: esphome.kia_niro_ev_obd2_send_elm327_command
data:
  command: "ATSH7E4"
```

Damit kannst du:
- ECU-Header setzen (`ATSH7E4`)
- Beliebige PIDs abfragen (`2201019`)
- AT-Konfiguration ändern (`ATSP6`, `ATST96`)
- Fehlerspeicher löschen (`04`) - **Vorsicht!**

Die Antwort erscheint im `raw` Text-Sensor (falls konfiguriert).

---

## Kia Niro EV Beispiel

Ein vollständiges Beispiel für den Kia Niro EV liegt in `example-kia-niro-ev.yaml`. Es zeigt:

- Raw-PID Text-Sensoren für die 3 Haupt-PIDs (BMS, BMS-Zusatz, Cluster)
- Automatischen ECU-Header-Wechsel (7E4, 7C6)
- Service-Call für beliebige Befehle aus HA
- HA Template-Sensor Beispiele zum Berechnen der Werte

**PID-Quellen für weitere Fahrzeuge:**
- [WiCAN Vehicle Profiles](https://github.com/meatpiHQ/wican-fw/tree/main/vehicle_profiles) -- Kia, Hyundai, BMW, VW, Tesla, etc.
- [Torque PID Files](https://torque-bhp.com/wiki/PIDs) -- Große Sammlung für viele Fahrzeuge

---

## Hub-Konfiguration

```yaml
elm327_ble:
  id: elm327_hub
  ble_client_id: elm327_ble_client
  service_uuid: "0000FFF0-0000-1000-8000-00805F9B34FB"
  char_tx_uuid: "0000FFF2-0000-1000-8000-00805F9B34FB"
  char_rx_uuid: "0000FFF1-0000-1000-8000-00805F9B34FB"
  request_interval: 2s    # Optional, Default: 2s
  request_timeout: 5s     # Optional, Default: 5s
```

| Parameter | Pflicht | Default | Beschreibung |
|---|---|---|---|
| `ble_client_id` | ja | - | ID des `ble_client` |
| `service_uuid` | ja | - | BLE Service UUID des ELM327 |
| `char_tx_uuid` | ja | - | Write Characteristic (Befehle senden) |
| `char_rx_uuid` | ja | - | Notify Characteristic (Antworten empfangen) |
| `request_interval` | nein | `2s` | Abstand zwischen PID-Abfragen |
| `request_timeout` | nein | `5s` | Timeout wenn keine Antwort kommt |

---

## Home Assistant Dashboard

Ein fertiges Dashboard liegt in `dashboard-ducato-obd2.yaml`.

**Installation:**

1. Home Assistant → Einstellungen → Dashboards → Dashboard hinzufügen
2. Neues Dashboard öffnen → oben rechts ⋮ → **Raw-Konfigurationseditor**
3. Inhalt von `dashboard-ducato-obd2.yaml` einfügen → Speichern

**Enthält:**

- Nadel-Gauges für Drehzahl, Geschwindigkeit, Motorlast
- Temperatur-Gauges (Kühlmittel, Öl, Ansaugluft)
- Kraftstoff-Anzeige mit Verbrauch
- Bedingte Fehlercode-Karte (grün wenn leer, Warnung bei DTCs)
- History-Graphen für alle Werte
- Kein HACS nötig -- nur Standard Home Assistant Karten

> **Hinweis:** Die Entity-IDs im Dashboard basieren auf dem Gerätenamen `ducato-obd2`. Falls du einen anderen Namen verwendest, passe die IDs per Suchen & Ersetzen an: `ducato_obd2` → `dein_name`.

---

## ELM327-Initialisierung

Bei BLE-Connect sendet die Component automatisch folgende Init-Sequenz:

```
ATZ       → Reset des ELM327
ATE0      → Echo aus
ATL0      → Linefeed aus
ATS0      → Spaces aus (kompakte Antworten)
ATH0      → Headers aus
ATSP0     → Automatische Protokollerkennung
0100      → Erste Abfrage (löst Protokoll-Erkennung aus, 5s warten)
```

Danach beginnt der zyklische PID-Abfrage-Modus. Falls Raw-PID Sensoren mit `header` konfiguriert sind, wird vor jeder Abfrage bei Bedarf automatisch `ATSH{header}` gesendet.

---

## Sicherheit

**Dieser Code ist rein lesend (read-only) und kann keine Schäden am Fahrzeug verursachen.**

- Alle OBD2-Abfragen sind **Mode 01** (Live-Daten lesen) oder **Mode 22** (erweiterte Diagnose lesen)
- Fehlerspeicher wird nur **gelesen** (Mode 03), nicht gelöscht
- AT-Befehle konfigurieren nur den ELM327-Chip, nicht das Fahrzeug
- Der `send_custom_command` Service-Call kann theoretisch auch schreibende Befehle senden -- mit Vorsicht verwenden!
- Im schlimmsten Fall trennt sich die BLE-Verbindung -- kein Effekt aufs Fahrzeug

---

## Troubleshooting

### BLE-Verbindung kommt nicht zustande

- Dongle aus OBD-Port ziehen und wieder einstecken
- MAC-Adresse prüfen (BLE-Scanner nochmal laufen lassen)
- Ist ein Handy/Tablet gleichzeitig mit dem Dongle verbunden? (Es ist nur **eine** BLE-Verbindung gleichzeitig möglich)
- Stimmen die BLE UUIDs? (Service, TX, RX nochmal mit BLE-Scanner Phase 2 oder nRF Connect prüfen)

### Alle PIDs liefern `NO DATA`

- Zündung muss **an** sein (mindestens Klemme 15)
- Die erste Abfrage nach Verbindung braucht 5-10 Sekunden (Protokoll-Erkennung)
- Falls dauerhaft: Eventuell falsches Protokoll. Per Service-Call `ATSP6` (CAN 500k) oder `ATSP7` (CAN 250k) testen

### Einzelne PIDs liefern `NO DATA`

- **Das ist normal.** Nicht jedes Fahrzeug unterstützt alle PIDs
- Diese Sensoren einfach aus der Config entfernen um den Abfragezyklus zu beschleunigen
- Typisch nicht unterstützt beim Ducato 250: Öltemperatur (`oil_temp`), Kraftstoffverbrauch (`fuel_rate`)

### Mode 22 PIDs liefern `NO DATA`

- Prüfe ob der richtige ECU-Header gesetzt ist (`header` Parameter)
- Manche ECUs brauchen zusätzliche Init-Befehle (z.B. `ATST96` für längeren Timeout)
- Per Service-Call testen: Erst `ATSH7E4`, dann `2201019` senden und den `raw` Text-Sensor beobachten

### `BUFFER FULL` oder `STOPPED`

- `request_interval` erhöhen (z.B. auf `3s` oder `5s`)
- Weniger Sensoren konfigurieren
- Bei langen Mode 22 Responses: `request_timeout` erhöhen

### Werte "nicht numerisch" in Home Assistant

- Die Component setzt automatisch `state_class: measurement` bei allen numerischen Sensoren
- Falls das Problem trotzdem auftritt: Entity in HA löschen und neu entdecken lassen, oder HA neustarten

---

## Optimierung

- **Nicht unterstützte PIDs entfernen:** PIDs die `NO DATA` liefern einfach aus der YAML löschen. Weniger Sensoren = schnellerer Zyklus
- **`request_interval` anpassen:**
  - `1s` = schnell, aber evtl. instabil bei vielen Sensoren
  - `2s` = Standard, guter Kompromiss
  - `5s` = langsam, aber sehr stabil
- **Rechenbeispiel:** 10 Sensoren x 2s = 20s pro komplettem Durchlauf
- **ECU-Header gruppieren:** Sensoren mit gleichem Header hintereinander konfigurieren, um Header-Wechsel zu minimieren

---

## Kompatibilität

| Getestet mit | Status |
|---|---|
| Veepeak OBDCheck BLE+ | funktioniert |
| M5Stack ATOM S3 (ESP32-S3) | funktioniert |
| Fiat Ducato 250 (2.3 Multijet) | funktioniert |
| Framework: Arduino | funktioniert |
| Framework: ESP-IDF | funktioniert |

Beide ESPHome-Frameworks werden unterstützt:

```yaml
# Arduino (Standard)
esp32:
  framework:
    type: arduino

# ESP-IDF (kleinere Firmware, mehr Flash-Platz frei)
esp32:
  framework:
    type: esp-idf
```

Sollte mit jedem **ELM327-BLE-Dongle** und jedem **ESP32** funktionieren. Die BLE UUIDs variieren je nach Dongle -- daher immer per BLE-Scan prüfen.

> Du hast die Component mit einem anderen Dongle/Fahrzeug getestet? Erstelle gerne ein [Issue](https://github.com/rubenmuehlhans/esphome-obd2-ble/issues) oder einen PR um die Kompatibilitätsliste zu erweitern!

---

## Quellen

- [ESPHome External Components](https://esphome.io/components/external_components.html)
- [ESPHome BLE Client](https://esphome.io/components/ble_client.html)
- [OBD2 PIDs (Wikipedia)](https://en.wikipedia.org/wiki/OBD-II_PIDs)
- [ELM327 Datenblatt](https://www.elmelectronics.com/DSheets/ELM327DSH.pdf)
- [WiCAN Vehicle Profiles](https://github.com/meatpiHQ/wican-fw/tree/main/vehicle_profiles) -- Fahrzeugspezifische PIDs

## Lizenz

Frei nutzbar. Keine Garantie, Nutzung auf eigene Verantwortung.
