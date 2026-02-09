# Fiat Ducato 250 OBD2 - ESPHome via Veepeak BLE

ESPHome-Konfiguration um OBD2-Fahrzeugdaten eines **Fiat Ducato 250** per **BLE** (Bluetooth Low Energy) in **Home Assistant** zu bringen.

## Hardware

| Komponente | Beschreibung |
|---|---|
| **M5Stack ATOM S3** | ESP32-S3 Mikrocontroller, kompakt, USB-C |
| **Veepeak OBDCheck BLE+** | ELM327-kompatibler OBD2-Dongle mit BLE |
| **Fiat Ducato 250** | Fahrzeug (Baujahr ab 2006, CAN-Bus) |

### Aufbau

```
Ducato OBD2-Port ←→ Veepeak BLE+ ←·····BLE·····→ ATOM S3 ←···WiFi···→ Home Assistant
                     (ELM327)                      (ESP32-S3)
```

Der ATOM S3 verbindet sich per BLE mit dem Veepeak, sendet OBD2-Abfragen und leitet die Daten per WiFi an Home Assistant weiter.

## Dateien

| Datei | Beschreibung |
|---|---|
| `ducato-obd2-atom-s3.yaml` | Haupt-Config: OBD2-Daten lesen und an HA senden |
| `ducato-ble-scanner.yaml` | Hilfs-Config: BLE-Geräte scannen (MAC/UUIDs finden) |

## Features

### 18 OBD2-Sensoren

| Sensor | PID | Einheit | Formel |
|---|---|---|---|
| Motortemperatur (Kühlmittel) | `0105` | °C | A - 40 |
| Drehzahl | `010C` | RPM | ((A*256)+B) / 4 |
| Geschwindigkeit | `010D` | km/h | A |
| Motorlast | `0104` | % | A*100 / 255 |
| Ansauglufttemperatur | `010F` | °C | A - 40 |
| Kraftstoffstand | `012F` | % | A*100 / 255 |
| Drosselklappe | `0111` | % | A*100 / 255 |
| Batteriespannung | `ATRV` | V | direkt vom ELM327 |
| Ansaugkrümmerdruck (MAP) | `010B` | kPa | A |
| Luftmassenmesser (MAF) | `0110` | g/s | ((A*256)+B) / 100 |
| Motorlaufzeit | `011F` | s | (A*256)+B |
| Motoröltemperatur | `015C` | °C | A - 40 |
| Umgebungstemperatur | `0146` | °C | A - 40 |
| ECU Spannung | `0142` | V | ((A*256)+B) / 1000 |
| Kraftstoffverbrauch | `015E` | L/h | ((A*256)+B) / 20 |
| Barometrischer Druck | `0133` | kPa | A |
| Abgasrückführung (AGR) | `012E` | % | A*100 / 255 |
| **Aktive Fehlercodes (DTC)** | `03` | Text | Mode 03 Decode |

### Weitere Entitäten in Home Assistant

| Entität | Typ | Beschreibung |
|---|---|---|
| Motor läuft | Binary Sensor | `true` wenn Drehzahl > 0 |
| ELM327 verbunden | Binary Sensor | BLE-Verbindungsstatus |
| Aktive Fehlercodes | Text Sensor | z.B. `P0123, P0456` oder `Keine Fehler` |
| Letzte ELM327 Antwort | Text Sensor | Debug: Rohantwort vom ELM327 |
| WiFi Signal | Sensor | RSSI in dBm |
| Laufzeit | Sensor | Uptime des ATOM S3 |
| IP-Adresse | Text Sensor | aktuelle IP |
| WLAN | Text Sensor | verbundenes Netzwerk |

## Installation

### Schritt 1: BLE-Scan (einmalig)

Beim ersten Mal musst du die BLE-Daten deines Veepeak ermitteln.

1. `ducato-ble-scanner.yaml` in ESPHome flashen
2. Veepeak in den OBD2-Port stecken, Zündung an
3. Logs beobachten -- nach `VEEPEAK`, `IOS-Vlink`, `V-LINK` oder `OBDII` suchen
4. **MAC-Adresse** und **Service UUID** notieren

Optional: Mit der App **nRF Connect** (iOS/Android) den Veepeak verbinden und die **Characteristics** prüfen:
- Eine Characteristic mit **Notify** = RX (Empfang, typisch FFF1)
- Eine Characteristic mit **Write** = TX (Senden, typisch FFF2)

### Schritt 2: Haupt-Config anpassen

In `ducato-obd2-atom-s3.yaml` die `substitutions` anpassen:

```yaml
substitutions:
  device_name: "ducato-obd2"
  friendly_name: "Ducato OBD2"
  elm327_mac: "66:1E:87:04:08:D0"          # ← Deine MAC-Adresse
  ble_service_uuid: "0000FFF0-0000-1000-8000-00805F9B34FB"  # ← Dein Service
  ble_char_rx: "0000FFF1-0000-1000-8000-00805F9B34FB"       # ← Notify Char
  ble_char_tx: "0000FFF2-0000-1000-8000-00805F9B34FB"       # ← Write Char
```

### Schritt 3: secrets.yaml

Stelle sicher, dass deine `secrets.yaml` folgende Einträge hat:

```yaml
wifi_ssid: "DeinWLAN"
wifi_password: "DeinWLANPasswort"
ap_password: "FallbackPasswort"
```

### Schritt 4: Flashen

In Home Assistant ESPHome Dashboard:
1. YAML einfügen oder als Datei hochladen
2. **Install** klicken
3. Beim ersten Mal per **USB** flashen (Plug-in: ESPHome Web)
4. Danach OTA-Updates per WiFi möglich

## Typische BLE UUIDs

Veepeak OBDCheck BLE+ hat je nach Hardware-Revision unterschiedliche UUIDs:

| Variante | Service UUID | RX (Notify) | TX (Write) |
|---|---|---|---|
| Variante A | `FFE0` | `FFE1` | `FFE1` |
| Variante B | `FFF0` | `FFF1` | `FFF2` |
| Variante C | `E7810A71...` | individuell | individuell |

Immer mit BLE-Scanner oder nRF Connect verifizieren!

## Funktionsweise im Detail

### ELM327-Initialisierung (bei BLE-Connect)

```
ATZ       → Reset des ELM327
ATE0      → Echo aus (keine Befehl-Rückgabe)
ATL0      → Linefeed aus
ATS0      → Leerzeichen aus (kompakte Antworten)
ATH0      → Headers aus
ATSP0     → Automatische Protokollerkennung
0100      → Erste Abfrage (löst Protokoll-Erkennung aus)
          → 5s warten bis CAN-Bus erkannt
```

### PID-Abfragezyklus

- **18 Abfragen** werden zyklisch alle **2 Sekunden** gesendet
- Pro Zyklus wird **eine** PID abgefragt, Antwort abgewartet
- Kompletter Durchlauf: ~36 Sekunden
- **Timeout:** Wenn nach 5s keine Antwort kommt, wird die nächste PID abgefragt
- PIDs die vom Fahrzeug nicht unterstützt werden, liefern `NO DATA` und werden ignoriert

### Antwort-Parsing

```
BLE Notify (FFF1) → Chunks sammeln → auf '>' warten (ELM327-Prompt)
→ Bereinigen (Whitespace, Steuerzeichen entfernen)
→ Typ erkennen:
    "43..."  → DTC-Fehlercodes dekodieren
    "...V"   → Batteriespannung (ATRV)
    "41XX.." → OBD2 Mode 01 PID-Daten nach Formel umrechnen
```

### DTC-Dekodierung (Fehlercodes)

Mode 03 Antwort: `43 XXYY XXYY ...`

Das erste Nibble (Halbbyte) bestimmt den Fehlertyp:

| Nibble | Prefix | Typ |
|---|---|---|
| 0-3 | P | Powertrain (Motor/Getriebe) |
| 4-5 | C | Chassis (Fahrwerk) |
| 6-7 | B | Body (Karosserie) |
| 8-B | U | Network (Kommunikation) |

Beispiel: `0123` → **P0123**, `4567` → **C1567**

## Nicht unterstützte PIDs

Nicht jedes Fahrzeug unterstützt alle PIDs. Der Ducato 250 (2.3 Multijet) unterstützt typischerweise **nicht**:

- Öltemperatur (`015C`) -- oft `NO DATA`
- Kraftstoffverbrauch (`015E`) -- oft `NO DATA`
- ECU Spannung (`0142`) -- manchmal `NO DATA`

Diese PIDs erzeugen lediglich `NO DATA`-Antworten und können bei Bedarf aus der PID-Liste entfernt werden, um den Abfragezyklus zu beschleunigen.

## Optimierung

### Abfragezyklus beschleunigen

Nicht unterstützte PIDs entfernen:

1. Flashen und Logs beobachten
2. PIDs die wiederholt `NO DATA` liefern, notieren
3. Aus dem `pid_cmds[]` Array entfernen
4. `num_pids` Zähler anpassen

Beispiel: Von 18 auf 10 PIDs → Zykluszeit von 36s auf 20s.

### Abfrageintervall ändern

In der `interval`-Sektion:

```yaml
interval:
  - interval: 2s    # ← Hier anpassen (Minimum ~1s empfohlen)
```

Zu schnelle Abfragen können den ELM327 überlasten und zu `BUFFER FULL` oder `STOPPED` Fehlern führen.

## Sicherheit

**Dieser Code ist rein lesend (read-only) und kann keine Schäden am Fahrzeug verursachen.**

- Alle OBD2-Abfragen sind **Mode 01** (Live-Daten lesen)
- Fehlerspeicher wird nur **gelesen** (Mode 03), nicht gelöscht (Mode 04 ist nicht implementiert)
- AT-Befehle konfigurieren nur den ELM327-Chip selbst, nicht das Fahrzeug
- Der ELM327 kann bauartbedingt keine Steuergeräte umprogrammieren
- Im schlimmsten Fall trennt sich die BLE-Verbindung -- kein Effekt aufs Fahrzeug

## Troubleshooting

### BLE-Verbindung kommt nicht zustande

- Veepeak Dongle aus OBD-Port ziehen und wieder einstecken
- Prüfen ob die MAC-Adresse stimmt (BLE-Scanner nochmal laufen lassen)
- Sicherstellen, dass kein Handy/Tablet gleichzeitig mit dem Veepeak verbunden ist (nur eine BLE-Verbindung möglich)

### Alle PIDs liefern `NO DATA`

- Zündung muss **an** sein (mindestens Klemme 15)
- Falsches OBD-Protokoll: `ATSP0` sollte Auto-Erkennung machen, falls nicht → `ATSP6` (CAN 500k) oder `ATSP7` (CAN 250k) testen
- Erste Abfrage nach Verbindung braucht oft 5-10s (Protokoll-Erkennung)

### Einzelne PIDs liefern `NO DATA`

- Normal! Nicht jedes Fahrzeug unterstützt alle PIDs
- Diese PIDs aus der Liste entfernen um den Zyklus zu beschleunigen

### `BUFFER FULL` oder `STOPPED` Fehler

- Abfrageintervall erhöhen (z.B. von 2s auf 3s)
- Weniger PIDs gleichzeitig abfragen

### Timeout-Meldungen im Log

- `Antwort-Timeout, mache weiter...` erscheint wenn eine PID nicht antwortet
- Normal bei nicht unterstützten PIDs oder wenn der Motor aus ist
- Der Zyklus setzt automatisch mit der nächsten PID fort

### WiFi-Verbindung instabil

- `fast_connect: true` ist gesetzt (verbindet ohne vollständigen Scan)
- Fallback-AP: Bei WiFi-Ausfall erstellt der ATOM S3 ein eigenes WLAN (`Ducato-OBD2-Fallback`)
- Captive Portal erreichbar unter `192.168.4.1`

## Webinterface

Der ATOM S3 stellt ein Webinterface auf Port 80 bereit:
- `http://<IP-Adresse>/` -- Übersicht aller Sensoren
- Erreichbar im lokalen Netzwerk oder über den Fallback-AP

## Lizenz

Dieses Projekt ist frei nutzbar. Keine Garantie, Nutzung auf eigene Verantwortung.

## Quellen

- [ESPHome BLE Client](https://esphome.io/components/ble_client.html)
- [ESPHome BLE Sensor](https://esphome.io/components/sensor/ble_client.html)
- [OBD2 PIDs (Wikipedia)](https://en.wikipedia.org/wiki/OBD-II_PIDs)
- [ELM327 AT Commands](https://www.elmelectronics.com/DSheets/ELM327DSH.pdf)
