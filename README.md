# ESPHome OBD2 BLE - ELM327 Custom Component

![ESPHome CI](https://github.com/rubenmuehlhans/esphome-obd2-ble/actions/workflows/ci.yml/badge.svg)

ESPHome Custom Component zum Auslesen von OBD2-Fahrzeugdaten per BLE (Bluetooth Low Energy) mit ELM327-kompatiblen Dongles. 17 Live-Sensoren, Fehlerspeicher-Auslesen und fertiges Home Assistant Dashboard.

Getestet mit **Fiat Ducato 250** + **Veepeak OBDCheck BLE+** + **M5Stack ATOM S3**.

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
# Sensor hinzufügen = 2 Zeilen:
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
| `example-component.yaml` | Beispiel-Config mit Custom Component |
| `ducato-obd2-atom-s3.yaml` | Standalone-Config (ohne Component) |
| `ducato-ble-scanner.yaml` | BLE-Scanner (MAC/UUIDs ermitteln) |
| `dashboard-ducato-obd2.yaml` | Home Assistant Dashboard |
| `secrets.yaml.example` | Vorlage fuer secrets.yaml |

## Schnellstart (Custom Component)

### 1. BLE-Daten ermitteln (einmalig)

MAC-Adresse und UUIDs deines ELM327-Dongles herausfinden:

- **Option A:** `ducato-ble-scanner.yaml` flashen und Logs lesen
- **Option B:** App **nRF Connect** (iOS/Android) nutzen

Typische Veepeak UUIDs:

| Variante | Service | RX (Notify) | TX (Write) |
|---|---|---|---|
| A | `FFE0` | `FFE1` | `FFE1` |
| B | `FFF0` | `FFF1` | `FFF2` |

### 2. Config erstellen

```yaml
external_components:
  - source: github://DEIN_USER/esphome-obd2-ble
    components: [elm327_ble]

esp32_ble_tracker:
  scan_parameters:
    active: true

ble_client:
  - mac_address: "AA:BB:CC:DD:EE:FF"  # Deine MAC
    id: elm327_ble_client

elm327_ble:
  id: elm327_hub
  ble_client_id: elm327_ble_client
  service_uuid: "0000FFF0-0000-1000-8000-00805F9B34FB"
  char_tx_uuid: "0000FFF2-0000-1000-8000-00805F9B34FB"
  char_rx_uuid: "0000FFF1-0000-1000-8000-00805F9B34FB"

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

text_sensor:
  - platform: elm327_ble
    type: dtc
    name: "Fehlercodes"

binary_sensor:
  - platform: elm327_ble
    type: engine_running
    name: "Motor laeuft"
```

### 3. secrets.yaml anlegen

```bash
cp secrets.yaml.example secrets.yaml
# Dann Werte eintragen
```

### 4. Flashen

In Home Assistant ESPHome Dashboard: YAML einfuegen, Install klicken.
Erstes Mal per USB, danach OTA per WiFi.

## Verfuegbare Sensor-Typen

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
| `intake_map` | Ansaugkruemmerdruck | `0x0B` | kPa | A |
| `maf` | Luftmassenmesser | `0x10` | g/s | ((A*256)+B) / 100 |
| `engine_runtime` | Motorlaufzeit | `0x1F` | s | (A*256)+B |
| `oil_temp` | Motoroeltemperatur | `0x5C` | °C | A - 40 |
| `ambient_temp` | Umgebungstemperatur | `0x46` | °C | A - 40 |
| `ecu_voltage` | ECU Spannung | `0x42` | V | ((A*256)+B) / 1000 |
| `fuel_rate` | Kraftstoffverbrauch | `0x5E` | L/h | ((A*256)+B) / 20 |
| `baro_pressure` | Barometrischer Druck | `0x33` | kPa | A |
| `egr` | Abgasrueckfuehrung | `0x2E` | % | A*100 / 255 |

### Eigene PIDs abfragen

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

### binary_sensor (platform: elm327_ble)

| type | Beschreibung |
|---|---|
| `connected` | ELM327 BLE-Verbindung hergestellt und initialisiert |
| `engine_running` | Motor laeuft (Drehzahl > 0) |

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
| `ble_client_id` | ja | - | ID des ble_client |
| `service_uuid` | ja | - | BLE Service UUID des ELM327 |
| `char_tx_uuid` | ja | - | Write Characteristic (Befehle senden) |
| `char_rx_uuid` | ja | - | Notify Characteristic (Antworten empfangen) |
| `request_interval` | nein | `2s` | Abstand zwischen PID-Abfragen |
| `request_timeout` | nein | `5s` | Timeout wenn keine Antwort kommt |

## Home Assistant Dashboard

Fertiges Dashboard in `dashboard-ducato-obd2.yaml`:

1. Home Assistant -> Einstellungen -> Dashboards -> Dashboard hinzufuegen
2. Dashboard oeffnen -> oben rechts 3 Punkte -> **Raw-Konfigurationseditor**
3. Inhalt von `dashboard-ducato-obd2.yaml` einfuegen, Speichern

Features:
- Nadel-Gauges fuer Drehzahl, Geschwindigkeit, Motorlast
- Temperatur-Gauges (Kuehlmittel, Oel, Ansaugluft)
- Kraftstoff-Anzeige mit Verbrauch
- Bedingte Fehlercode-Karte (gruen wenn leer, Warnung bei DTCs)
- History-Graphen fuer alle Werte
- Kein HACS noetig -- nur Standard HA Karten

## ELM327-Initialisierung

Bei BLE-Connect sendet die Component automatisch:

```
ATZ       -> Reset
ATE0      -> Echo aus
ATL0      -> Linefeed aus
ATS0      -> Spaces aus
ATH0      -> Headers aus
ATSP0     -> Automatische Protokollerkennung
0100      -> Erste Abfrage (loest Protokoll-Erkennung aus, 5s warten)
```

## Sicherheit

**Dieser Code ist rein lesend (read-only) und kann keine Schaeden am Fahrzeug verursachen.**

- Alle OBD2-Abfragen sind **Mode 01** (Live-Daten lesen)
- Fehlerspeicher wird nur **gelesen** (Mode 03), nicht geloescht
- AT-Befehle konfigurieren nur den ELM327-Chip, nicht das Fahrzeug
- Im schlimmsten Fall trennt sich die BLE-Verbindung -- kein Effekt aufs Fahrzeug

## Troubleshooting

### BLE-Verbindung kommt nicht zustande

- Dongle aus OBD-Port ziehen und wieder einstecken
- MAC-Adresse pruefen (BLE-Scanner nochmal laufen lassen)
- Kein Handy/Tablet gleichzeitig mit dem Dongle verbunden? (nur eine BLE-Verbindung moeglich)

### Alle PIDs liefern `NO DATA`

- Zuendung muss **an** sein (mindestens Klemme 15)
- Erste Abfrage nach Verbindung braucht 5-10s (Protokoll-Erkennung)

### Einzelne PIDs liefern `NO DATA`

- Normal -- nicht jedes Fahrzeug unterstuetzt alle PIDs
- Diese Sensoren aus der Config entfernen um den Zyklus zu beschleunigen

### `BUFFER FULL` oder `STOPPED`

- `request_interval` erhoehen (z.B. `3s`)
- Weniger Sensoren konfigurieren

### Werte nicht numerisch in HA

- Alle Sensoren haben `state_class: measurement` (wird automatisch von der Component gesetzt)
- Falls das Problem trotzdem auftritt: Entity in HA loeschen und neu entdecken lassen

## Optimierung

- Nicht unterstuetzte PIDs (die `NO DATA` liefern) aus der Config entfernen
- `request_interval` anpassen: 1s (schnell, evtl. instabil) bis 5s (langsam, stabil)
- Beispiel: 10 Sensoren x 2s = 20s pro Durchlauf

## Kompatibilitaet

| Getestet mit | Status |
|---|---|
| Veepeak OBDCheck BLE+ | funktioniert |
| M5Stack ATOM S3 (ESP32-S3) | funktioniert |
| Fiat Ducato 250 (2.3 Multijet) | funktioniert |

Sollte mit jedem ELM327-BLE-Dongle und jedem ESP32 funktionieren. BLE UUIDs variieren je nach Dongle -- immer per BLE-Scan pruefen.

## Quellen

- [ESPHome External Components](https://esphome.io/components/external_components.html)
- [ESPHome BLE Client](https://esphome.io/components/ble_client.html)
- [OBD2 PIDs (Wikipedia)](https://en.wikipedia.org/wiki/OBD-II_PIDs)
- [ELM327 Datenblatt](https://www.elmelectronics.com/DSheets/ELM327DSH.pdf)

## Lizenz

Frei nutzbar. Keine Garantie, Nutzung auf eigene Verantwortung.
