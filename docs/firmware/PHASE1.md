# Phase 1 — Prototype Firmware Plan

> Each step requires explicit confirmation before proceeding.
> Current status tracked in README.md checklist.

---

## Step-by-step implementation order

### ✅ Step 1 — Scaffold (complete)
- `platformio.ini` with ESP32-C6, espressif32@6.10.0, all libraries
- `partitions.csv` with dual OTA banks + NVS + SPIFFS
- `config.h` with all pins, addresses, and threshold defaults
- `storage/nvs_config.h/.cpp` — persistent config store
- `connectivity/wifi_manager.h/.cpp` — captive portal provisioning
- `alerts/alert_engine.h/.cpp` — state machine skeleton
- `main.cpp` — boot sequence + FreeRTOS task launch
- Full documentation scaffold

**Confirm before Step 2**

---

### ⬜ Step 2 — Sensor drivers
Implement each sensor driver in `main/sensors/`:

| File | Sensor | Interface | Notes |
|---|---|---|---|
| `sht40.cpp` | SHT40 | I²C 0x44 | Temp + humidity |
| `bmp280.cpp` | BMP280 | I²C 0x77 | Pressure |
| `scd41.cpp` | SCD41 | I²C 0x62 | CO₂, 5 s measurement cycle |
| `sgp41.cpp` | SGP41 | I²C 0x59 | VOC + NOx, needs SHT40 data |
| `veml7700.cpp` | VEML7700 | I²C 0x10 | Lux |
| `mq2.cpp` | MQ-2 | ADC GPIO0 | Smoke, requires 60 s warm-up |
| `pmsa003i.cpp` | PMSA003I | I²C 0x12 | PM1/2.5/10 |
| `ld2410c.cpp` | LD2410C | UART | Presence + distance |
| `ics43434.cpp` | ICS-43434 | I²S | Noise dB(A), Leq |

Each driver populates the shared `g_readings` struct.
Each driver has a `begin()` and `read(SensorReadings&)` method.

**Confirm before Step 3**

---

### ⬜ Step 3 — Outputs
Implement output drivers:

| File | Output | Notes |
|---|---|---|
| `outputs/led.cpp` | WS2812B | FastLED, setAlert(), pulse(), setColor() |
| `outputs/buzzer.cpp` | Piezo | PWM, alarm(), beep(n), stop() |
| `outputs/dfplayer.cpp` | DFPlayer Mini | UART, play(track), playForSource() |

Audio file naming convention for microSD:
```
/01.mp3  — Smoke detected in [room]
/02.mp3  — CO2 high
/03.mp3  — CO2 critical
/04.mp3  — Temperature high
/05.mp3  — Humidity high
/06.mp3  — Poor air quality
/07.mp3  — Particulates high
/08.mp3  — High noise level
/09.mp3  — All clear
/10.mp3  — WiFi connected
/11.mp3  — Setup mode
```

**Confirm before Step 4**

---

### ⬜ Step 4 — OLED display
Implement `display/oled.cpp`:

**Screen 0 — Indoor readings (default)**
```
┌──────────────────────────┐
│ Kitchen           🟢     │
│ Temp: 22.3°C  Hum: 48%  │
│ CO₂:  612ppm  AQI: Good  │
│ PM2.5: 8µg    Noise: 42dB│
└──────────────────────────┘
```

**Screen 1 — Alert status**
```
┌──────────────────────────┐
│ ⚠ WARNING                │
│ CO₂ elevated: 1240 ppm  │
│ Open a window            │
│ Smoke: OK  CO: OK        │
└──────────────────────────┘
```

**Screen 2 — WiFi + system info**
```
┌──────────────────────────┐
│ 192.168.1.42   -68dBm   │
│ MQTT: connected          │
│ v0.1.0  ID:42  Uptime:4h │
│ Weather: 72°F Partly ☁  │
└──────────────────────────┘
```

**Confirm before Step 5**

---

### ⬜ Step 5 — ESP-NOW encrypted mesh
Implement `alerts/espnow_mesh.cpp`:

- Init ESP-NOW with PMK (AES-128)
- Register peers from NVS (scanned on first boot)
- `broadcast(EspNowAlertPayload)` — sends alert to all peers × 3
- Receive callback → `AlertEngine::onRemoteAlert()`
- Peer discovery: cubes broadcast a hello packet on boot

Security notes:
- PMK stored in `config.h` — **must be changed** before production
- LMK per-peer derived from PMK + cube MAC
- Message deduplication by timestamp to prevent replay

**Confirm before Step 6**

---

### ⬜ Step 6 — MQTT + Home Assistant discovery
Implement `connectivity/mqtt_client.cpp`:

**Topic structure:**
```
envcube/kitchen/temperature    → {"value": 22.3, "unit": "C"}
envcube/kitchen/humidity       → {"value": 48.1, "unit": "%"}
envcube/kitchen/co2            → {"value": 612, "unit": "ppm"}
envcube/kitchen/smoke          → {"value": 820, "raw": true}
envcube/kitchen/pm25           → {"value": 8, "unit": "ug/m3"}
envcube/kitchen/voc_index      → {"value": 45}
envcube/kitchen/noise_db       → {"value": 42.1, "unit": "dBA"}
envcube/kitchen/presence       → {"value": true, "distance_cm": 180}
envcube/kitchen/lux            → {"value": 340}
envcube/kitchen/pressure       → {"value": 1013.2, "unit": "hPa"}
envcube/kitchen/alert          → {"level": 0, "message": "All clear"}
envcube/kitchen/status         → {"online": true, "rssi": -68, "uptime": 14400}
envcube/kitchen/heartbeat      → {"ts": 1735000000}
```

**HA auto-discovery topics:**
```
homeassistant/sensor/envcube_kitchen_temperature/config → {...}
homeassistant/binary_sensor/envcube_kitchen_smoke/config → {...}
homeassistant/binary_sensor/envcube_kitchen_presence/config → {...}
```

Published once on connect — HA immediately shows all entities.

**Confirm before Step 7**

---

### ⬜ Step 7 — OTA + weather fetch
`connectivity/ota_manager.cpp` — ArduinoOTA already wired in WiFi manager.
`connectivity/weather.cpp` — Open-Meteo REST fetch, no API key.

Weather endpoint:
```
https://api.open-meteo.com/v1/forecast
  ?latitude=33.6846
  &longitude=-117.826
  &current=temperature_2m,relative_humidity_2m,weather_code,uv_index
  &temperature_unit=fahrenheit
```

**Confirm before Step 8**

---

### ⬜ Step 8 — Integration test + HA dashboard
- Flash both prototype cubes
- Provision via captive portal
- Verify all sensors reading in HA
- Test smoke alert propagation across cubes (use lighter at safe distance)
- Test OTA update from IDE
- Test factory reset (10 s button hold)
- Document calibration values for MQ-2

**Sign off Phase 1 → begin Phase 2 planning**

---

## Development workflow

```bash
# Build
pio run -e esp32c6

# Flash via USB
pio run -e esp32c6 --target upload

# Monitor serial
pio device monitor -e esp32c6

# Flash via OTA (cube must be on WiFi)
pio run -e esp32c6-ota --target upload

# Run unit tests (native)
pio test -e native
```

## Branching strategy
```
main          — stable, flashed to prototype cubes
develop       — active development
feature/*     — individual sensor/module branches
hotfix/*      — urgent fixes
```
