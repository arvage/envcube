# EnvCube 🟢🟡🔴

> Smart modular environmental monitor — replaces every smoke and CO detector in your home with an always-on, mains-powered, voice-alert cube.

[![Phase](https://img.shields.io/badge/phase-1%20prototype-teal)](docs/firmware/PHASE1.md)
[![Hardware](https://img.shields.io/badge/MCU-ESP32--C6-blue)](docs/hardware/BOM.md)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)

---

## What is EnvCube?

EnvCube is a modular, offline-first environmental monitor designed to replace the smoke detectors and CO alarms that every US home is legally required to have — but that everyone hates because they beep at 3am when the battery dies.

**Key differentiators:**
- **Mains powered** via USB-C with silent LiPo backup — no batteries, no 3am beeping, ever
- **Voice alerts** — speaker says *"Smoke detected in Kitchen"*, not just a generic beep
- **Location-aware mesh** — ESP-NOW broadcasts room name to all cubes simultaneously
- **Offline-first** — all alerts, LED, buzzer, and speaker work with zero internet
- **Modular pods** — snap on extra sensors with pogo pin + magnetic connectors
- **Cloud watchdog** — alerts your phone if a cube goes silent or offline

---

## Hardware

| Component | Details |
|---|---|
| MCU | ESP32-C6 (WiFi 6, Thread, Zigbee, ESP-NOW, BLE) |
| Power | USB-C 5V mains + LiPo backup cell |
| Status | WS2812B RGB LED (Green / Amber / Red) |
| Alert | Passive piezo buzzer + DFPlayer Mini + 28mm speaker |
| Display | SSD1306 0.96" OLED 128×64 |
| Pod interface | 4× pogo pin + magnetic connector (I²C + power + 1-Wire ID) |

### Sensor Pods

| Pod | Sensors | Interface |
|---|---|---|
| Thermal | SHT40 (temp/hum) + BMP280 (pressure) | I²C |
| Smoke + CO₂ | MQ-2 (smoke) + SCD41 (NDIR CO₂) | ADC + I²C |
| Air Quality | SGP41 (VOC/NOx) + VEML7700 (lux) | I²C |
| Presence | HLK-LD2410C (24GHz mmWave) | UART |
| Particulate | PMSA003I (PM1/2.5/10) | I²C |
| Noise | ICS-43434 MEMS mic (dB SPL) | I²S |

See [BOM](docs/hardware/BOM.md) for full component list with part numbers and suppliers.

---

## Firmware

Built with **Arduino framework on ESP32-C6** via PlatformIO (espressif32 @ 6.10.0).

### Phase 1 — Prototype (current)
- [x] Project scaffold + PlatformIO config
- [ ] NVS config (room name, thresholds, credentials)
- [ ] WiFi manager + captive portal provisioning
- [ ] Sensor drivers (SHT40, SCD41, SGP41, MQ-2, LD2410C, PMSA003I, ICS-43434)
- [ ] Alert state machine (Green → Amber → Red)
- [ ] LED + buzzer + DFPlayer voice alerts
- [ ] OLED display (offline screen)
- [ ] ESP-NOW encrypted mesh
- [ ] MQTT + Home Assistant auto-discovery
- [ ] OTA firmware update
- [ ] Weather fetch (Open-Meteo)

See [PHASE1.md](docs/firmware/PHASE1.md) for detailed step-by-step plan.

---

## Getting Started

### Prerequisites
- [VS Code](https://code.visualstudio.com/) + [PlatformIO extension](https://platformio.org/install/ide?install=vscode)
- ESP32-C6-DevKitC-1 board
- USB-C cable

### Clone and build

```bash
git clone https://github.com/YOUR_USERNAME/envcube.git
cd envcube
```

Open in VS Code → PlatformIO will auto-install dependencies.

Flash:
```bash
pio run --target upload --environment esp32c6
```

Monitor serial:
```bash
pio device monitor --environment esp32c6
```

### First-time provisioning

1. Power the cube — RGB LED pulses **blue** (provisioning mode)
2. Connect your phone to WiFi network **`EnvCube-Setup`**
3. Browser opens automatically → enter your WiFi credentials + room name
4. Cube reboots, LED turns **green** when connected

---

## Repository structure

```
envcube/
├── firmware/
│   ├── main/
│   │   ├── main.cpp              # Boot, task launch
│   │   ├── sensors/              # One driver per sensor
│   │   ├── alerts/               # Threshold engine + ESP-NOW mesh
│   │   ├── outputs/              # LED, buzzer, DFPlayer
│   │   ├── display/              # OLED layouts
│   │   ├── connectivity/         # WiFi, MQTT, OTA, weather
│   │   ├── storage/              # NVS config wrapper
│   │   └── power/                # Sleep modes, LiPo monitor
│   ├── test/                     # Unit tests
│   ├── platformio.ini
│   └── partitions.csv
├── docs/
│   ├── hardware/
│   │   ├── BOM.md                # Full bill of materials
│   │   ├── PINOUT.md             # ESP32-C6 pin assignments
│   │   └── POD_DESIGN.md        # Pod enclosure + sensor placement
│   └── firmware/
│       ├── PHASE1.md             # Phase 1 step-by-step plan
│       ├── ARCHITECTURE.md      # Firmware module overview
│       ├── MQTT_TOPICS.md       # Full MQTT topic reference
│       └── THRESHOLDS.md        # Alert threshold values
└── tools/
    └── audio/                    # Voice clip source files
```

---

## Contributing

This is a private product development repository. See [CONTRIBUTING.md](CONTRIBUTING.md) for internal workflow guidelines.

---

## License

MIT — see [LICENSE](LICENSE)
