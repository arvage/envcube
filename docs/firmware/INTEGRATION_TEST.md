# Phase 1 Integration Test Checklist

Run through this checklist with both prototype cubes before signing off Phase 1.
Check each item. Document any failures with serial output for debugging.

---

## Pre-flight

- [ ] Both ESP32-C6-DevKitC-1 boards flashed with latest firmware
- [ ] All sensor breakouts wired per [PINOUT.md](../hardware/PINOUT.md)
- [ ] MicroSD card with 11 MP3 voice clips inserted in DFPlayer
- [ ] Both cubes powered via USB-C
- [ ] Home Assistant running with Mosquitto MQTT broker add-on
- [ ] Serial monitor open on each cube (115200 baud)

---

## 1. Boot sequence

### Cube 1
- [ ] Serial shows `EnvCube v0.1.0 Phase 1 prototype`
- [ ] OLED shows boot splash with version
- [ ] LED briefly white during boot
- [ ] DFPlayer initialises (`[DFPlayer] Ready — N files on SD card`)
- [ ] All sensors initialise (check `[SHT40]`, `[SCD41]`, `[SGP41]` etc.)
- [ ] MQ-2 warm-up message appears: `warming up for 60 seconds`

### Cube 2
- [ ] Same as above

---

## 2. WiFi provisioning (first-time setup)

- [ ] LED pulses blue on unprovisionned cube
- [ ] OLED shows provisioning screen with SSID `EnvCube-Setup`
- [ ] Phone connects to `EnvCube-Setup` WiFi network
- [ ] Browser auto-opens captive portal (or navigate to `192.168.4.1`)
- [ ] Portal shows WiFi scan results + room name + MQTT + location fields
- [ ] Enter: WiFi credentials, room name (e.g. `Kitchen`), MQTT broker IP,
      latitude + longitude
- [ ] Cube reboots and connects to WiFi
- [ ] Serial shows `[WiFi] Connected — IP: 192.168.x.x`
- [ ] LED turns green
- [ ] DFPlayer says `"WiFi connected"`
- [ ] Buzzer confirmation beep

**Repeat for Cube 2 with room name `Bedroom`**

---

## 3. Sensor readings (after 60 s MQ-2 warm-up)

Open serial monitor and verify all sensors reporting:

### Thermal pod (SHT40 + BMP280)
- [ ] `[SHT40] T=xx.x°C (raw=xx.x, offset=x.x) RH=xx.x%`
- [ ] `[BMP280] P=xxxx.x hPa`
- [ ] Temperature reading is plausible (within 2°C of room thermometer)
- [ ] Humidity reading is plausible (30–60% typical indoors)

### CO₂ pod (SCD41)
- [ ] `[SCD41] CO₂=xxx ppm` (typical indoor: 400–800 ppm)
- [ ] First valid reading appears within 10 s of boot
- [ ] Hold your breath near sensor — CO₂ should rise within 30 s

### Air quality pod (SGP41)
- [ ] `[SGP41] VOC=xxx NOx=x` after 10 s conditioning
- [ ] VOC index 80–150 is normal indoors
- [ ] Wave hand near sensor — VOC index should react briefly

### Smoke pod (MQ-2)
- [ ] `[MQ-2] Raw=xxx status=clear` after warm-up
- [ ] ⚠ **Test carefully**: briefly hold a lit match 30cm away (outdoors or
      ventilated area) — raw value should spike above threshold
- [ ] Alert triggers: LED red, buzzer alarms, DFPlayer says `"Smoke detected"`

### Particulate pod (PMSA003I)
- [ ] `[PMSA003I] PM1=x PM2.5=x PM10=x µg/m³`
- [ ] Values < 12 µg/m³ PM2.5 = good indoor air quality

### Presence pod (LD2410C)
- [ ] `[LD2410C] PRESENT dist=xxxcm` when you're in the room
- [ ] `[LD2410C] empty dist=0cm` when you leave
- [ ] Sit perfectly still — should still show PRESENT (mmWave, not PIR)

### Noise pod (ICS-43434)
- [ ] `[ICS43434] Leq=xx.x dB(A)` — typical quiet room: 30–45 dB
- [ ] Clap hands — Leq should spike 10–20 dB

### Ambient light (VEML7700)
- [ ] `[VEML7700] Lux=xxx` — cover sensor with hand, value should drop near 0

---

## 4. Alert state machine

### Warning level
- [ ] Breathe CO₂ near SCD41 until reading exceeds `THRESH_CO2_WARN` (1000 ppm)
- [ ] LED turns amber
- [ ] Serial: `[Alert] Level changed: 1`
- [ ] OLED Screen 1 shows WARNING banner

### Alert level
- [ ] Continue until CO₂ exceeds `THRESH_CO2_ALERT` (2000 ppm)
- [ ] LED turns red
- [ ] Buzzer beeps twice

### All clear
- [ ] Ventilate — CO₂ drops below thresholds
- [ ] LED returns to green
- [ ] Serial: `[Alert] Level changed: 0`
- [ ] DFPlayer says `"All clear"` (if voice enabled)

### Presence suppression
- [ ] Leave the room
- [ ] LD2410C shows `empty`
- [ ] Trigger a WARNING-level event (humidity, VOC)
- [ ] Verify alert is suppressed (no buzzer, amber LED only if configured)

---

## 5. ESP-NOW mesh

- [ ] Both cubes powered and on same WiFi channel
- [ ] Cube 1 serial: `[ESP-NOW] New peer: Bedroom (ID:x)`
- [ ] Cube 2 serial: `[ESP-NOW] New peer: Kitchen (ID:x)`
- [ ] OLED Screen 2 shows `P:1` (1 peer)

### Cross-cube alert test
- [ ] Trigger smoke alert on Cube 1 (Kitchen)
- [ ] Within 1 second: Cube 2 (Bedroom) LED turns red
- [ ] Cube 2 buzzer alarms
- [ ] Cube 2 DFPlayer says `"Smoke detected"` (remote alert)
- [ ] Serial on Cube 2: `[ESP-NOW] ALERT from Kitchen level=3`

---

## 6. MQTT + Home Assistant

### Broker connection
- [ ] Serial: `[MQTT] Connected`
- [ ] MQTT Explorer (or similar) shows `envcube/kitchen/#` topics
- [ ] All sensor topics publishing with correct JSON format
- [ ] `envcube/kitchen/status` shows `{"online":true,...}`
- [ ] `envcube/kitchen/heartbeat` updating every 60 s

### HA auto-discovery
- [ ] In HA: Settings → Devices & Services → MQTT → Devices
- [ ] `Kitchen` device appears with all entities listed:
  - [ ] `sensor.kitchen_temperature`
  - [ ] `sensor.kitchen_humidity`
  - [ ] `sensor.kitchen_co2`
  - [ ] `sensor.kitchen_voc_index`
  - [ ] `sensor.kitchen_pm25`
  - [ ] `sensor.kitchen_noise_db`
  - [ ] `sensor.kitchen_lux`
  - [ ] `sensor.kitchen_pressure`
  - [ ] `binary_sensor.kitchen_smoke`
  - [ ] `binary_sensor.kitchen_presence`
  - [ ] `sensor.kitchen_alert_level`
- [ ] All entities showing live values (not unavailable)

### LWT watchdog test
- [ ] Unplug Cube 1 (Kitchen)
- [ ] Within 90 s: `envcube/kitchen/status` shows `{"online":false,...}`
- [ ] HA entity shows `unavailable`
- [ ] Plug back in — status returns to `online` within 10 s

---

## 7. OLED display

- [ ] Screen 0: indoor readings — all values updating
- [ ] Screen 1: alert status — correct level shown
- [ ] Screen 2: IP address, room name, uptime, peer count
- [ ] Button tap cycles screens
- [ ] After 30 s no button press: display dims
- [ ] Button press restores brightness

### Weather (if lat/lon configured)
- [ ] Screen 2 shows outdoor temperature + conditions after ~30 s
- [ ] Serial: `[Weather] xx.x°F xx%RH code:x UV:x.x`
- [ ] `envcube/kitchen/weather` publishing in MQTT

---

## 8. OTA update

- [ ] Cube connected to WiFi and on same network as dev machine
- [ ] In PlatformIO: switch to `esp32c6-ota` environment
- [ ] Run upload — serial shows `[OTA] Starting firmware update...`
- [ ] OLED shows progress bar
- [ ] LED flashes white during update
- [ ] Cube reboots with new firmware
- [ ] LED turns green on successful reboot

---

## 9. Factory reset

- [ ] Hold button for 10 seconds
- [ ] Serial: `[NVS] FACTORY RESET — clearing all stored config`
- [ ] Cube reboots into provisioning mode
- [ ] LED pulses blue
- [ ] OLED shows setup screen

---

## Sign-off

| Item | Status | Notes |
|---|---|---|
| Both cubes provisioned | | |
| All sensors reading | | |
| Alert state machine | | |
| ESP-NOW mesh alerts | | |
| MQTT + HA entities | | |
| LWT watchdog | | |
| OTA update | | |
| OLED all screens | | |
| Weather fetch | | |
| Factory reset | | |

**Phase 1 sign-off date:** _______________
**Signed off by:** _______________

---

## Known calibration items before Phase 2

1. **MQ-2 smoke baseline** — run `Mq2::calibrateCleanAir()` in fresh air
   and update `baseline_clean_air` in `mq2.cpp`
2. **SHT40 thermal offset** — compare reading against reference thermometer,
   adjust the `0.35f` multiplier in `sht40.cpp` line ~42
3. **SCD41 temperature offset** — if CO₂ readings drift high, adjust
   `setTemperatureOffset()` in `scd41.cpp` line ~38
4. **ICS-43434 sensitivity** — compare against a phone dB meter app,
   adjust `sensitivityOffset` in `ics43434.cpp` if readings are off by > 3 dB
