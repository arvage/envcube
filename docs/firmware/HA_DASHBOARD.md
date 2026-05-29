# Home Assistant Dashboard Guide

Complete Lovelace dashboard setup for EnvCube.
Replace `kitchen` and `bedroom` with your actual room slugs (lowercase, hyphens).

---

## Prerequisites

1. Mosquitto MQTT broker add-on installed and running
2. MQTT integration configured in HA (Settings → Integrations → MQTT)
3. Both cubes provisioned and connected — entities auto-created via discovery
4. File editor add-on (or Studio Code Server) for YAML editing

---

## Quick entity check

Before building the dashboard, verify entities exist:
```
Settings → Devices & Services → MQTT → Devices → Kitchen
```
All 11 entities should show live values. If any show `unavailable`,
check the cube is online and MQTT is publishing.

---

## Dashboard YAML

Go to your dashboard → Edit → Raw configuration editor, then paste:

```yaml
title: EnvCube
views:
  - title: Overview
    icon: mdi:home-thermometer
    cards:

      # ── Kitchen cube ─────────────────────────────────────────
      - type: vertical-stack
        cards:
          - type: markdown
            content: "## 🟢 Kitchen"

          - type: glance
            entities:
              - entity: sensor.kitchen_temperature
                name: Temp
              - entity: sensor.kitchen_humidity
                name: Humidity
              - entity: sensor.kitchen_co2
                name: CO₂
              - entity: sensor.kitchen_pm25
                name: PM2.5
              - entity: sensor.kitchen_noise_db
                name: Noise
              - entity: sensor.kitchen_lux
                name: Lux

          - type: entities
            entities:
              - entity: binary_sensor.kitchen_smoke
                name: Smoke
              - entity: binary_sensor.kitchen_presence
                name: Presence
              - entity: sensor.kitchen_voc_index
                name: VOC Index
              - entity: sensor.kitchen_nox_index
                name: NOx Index
              - entity: sensor.kitchen_alert_level
                name: Alert Level
              - entity: sensor.kitchen_pressure
                name: Pressure

      # ── Bedroom cube ─────────────────────────────────────────
      - type: vertical-stack
        cards:
          - type: markdown
            content: "## 🟢 Bedroom"

          - type: glance
            entities:
              - entity: sensor.bedroom_temperature
                name: Temp
              - entity: sensor.bedroom_humidity
                name: Humidity
              - entity: sensor.bedroom_co2
                name: CO₂
              - entity: sensor.bedroom_pm25
                name: PM2.5
              - entity: sensor.bedroom_noise_db
                name: Noise
              - entity: sensor.bedroom_lux
                name: Lux

          - type: entities
            entities:
              - entity: binary_sensor.bedroom_smoke
                name: Smoke
              - entity: binary_sensor.bedroom_presence
                name: Presence
              - entity: sensor.bedroom_voc_index
                name: VOC Index
              - entity: sensor.bedroom_alert_level
                name: Alert Level

  # ── Air Quality detail view ───────────────────────────────────
  - title: Air Quality
    icon: mdi:air-filter
    cards:
      - type: history-graph
        title: CO₂ (ppm)
        hours_to_show: 24
        entities:
          - entity: sensor.kitchen_co2
            name: Kitchen
          - entity: sensor.bedroom_co2
            name: Bedroom

      - type: history-graph
        title: Temperature (°C)
        hours_to_show: 24
        entities:
          - entity: sensor.kitchen_temperature
            name: Kitchen
          - entity: sensor.bedroom_temperature
            name: Bedroom

      - type: history-graph
        title: PM2.5 (µg/m³)
        hours_to_show: 24
        entities:
          - entity: sensor.kitchen_pm25
            name: Kitchen
          - entity: sensor.bedroom_pm25
            name: Bedroom

      - type: history-graph
        title: VOC Index
        hours_to_show: 24
        entities:
          - entity: sensor.kitchen_voc_index
            name: Kitchen
          - entity: sensor.bedroom_voc_index
            name: Bedroom

      - type: history-graph
        title: Noise dB(A)
        hours_to_show: 24
        entities:
          - entity: sensor.kitchen_noise_db
            name: Kitchen
          - entity: sensor.bedroom_noise_db
            name: Bedroom

  # ── Status view ───────────────────────────────────────────────
  - title: Status
    icon: mdi:information
    cards:
      - type: entities
        title: Cube Status
        entities:
          - entity: sensor.kitchen_status
            name: Kitchen Online
          - entity: sensor.bedroom_status
            name: Bedroom Online

      - type: logbook
        title: Alert History
        hours_to_show: 48
        entities:
          - sensor.kitchen_alert_level
          - sensor.bedroom_alert_level
          - binary_sensor.kitchen_smoke
          - binary_sensor.bedroom_smoke
```

---

## Alert automations

### Smoke detected — phone notification
```yaml
alias: EnvCube Smoke Alert
trigger:
  - platform: state
    entity_id: binary_sensor.kitchen_smoke
    to: "on"
  - platform: state
    entity_id: binary_sensor.bedroom_smoke
    to: "on"
action:
  - service: notify.mobile_app_your_phone
    data:
      title: "🚨 Smoke Detected"
      message: >
        Smoke detected in
        {{ trigger.to_state.attributes.friendly_name }}.
        Check immediately.
      data:
        push:
          sound: default
          badge: 1
mode: single
```

### Cube offline — watchdog alert
```yaml
alias: EnvCube Offline Alert
trigger:
  - platform: state
    entity_id:
      - sensor.kitchen_status
      - sensor.bedroom_status
    to: unavailable
    for:
      minutes: 2
action:
  - service: notify.mobile_app_your_phone
    data:
      title: "⚠️ EnvCube Offline"
      message: >
        {{ trigger.to_state.name }} has gone offline.
        Check power and WiFi.
mode: parallel
max: 2
```

### CO₂ high — ventilation reminder
```yaml
alias: EnvCube CO2 Warning
trigger:
  - platform: numeric_state
    entity_id: sensor.kitchen_co2
    above: 1000
    for:
      minutes: 5
action:
  - service: notify.mobile_app_your_phone
    data:
      title: "🌬️ Open a Window"
      message: >
        Kitchen CO₂ is {{ states('sensor.kitchen_co2') }} ppm.
        Ventilate the room.
mode: single
```

### Presence-aware night mode
```yaml
alias: EnvCube Night Mode
trigger:
  - platform: time
    at: "22:00:00"
condition:
  - condition: state
    entity_id: binary_sensor.bedroom_presence
    state: "on"
action:
  - service: mqtt.publish
    data:
      topic: envcube/bedroom/cmd/dim
      payload: "true"
mode: single
```

---

## MQTT Explorer setup

For debugging during hardware bring-up, install
[MQTT Explorer](https://mqtt-explorer.com) and connect to your broker.

Subscribe to `envcube/#` to see all cube traffic in real time.

Useful topics to watch:
```
envcube/kitchen/temperature    → live temp readings
envcube/kitchen/alert          → alert state changes
envcube/kitchen/heartbeat      → watchdog pings (every 60 s)
envcube/kitchen/status         → online/offline (LWT)
```

---

## Sensor threshold tuning via MQTT

Override default thresholds without reflashing:
```bash
# Raise CO₂ warning threshold to 1200 ppm
mosquitto_pub -h YOUR_BROKER_IP \
  -t envcube/kitchen/cmd/threshold \
  -m '{"sensor":"co2_warn","value":1200}'

# Lower smoke alert threshold (more sensitive)
mosquitto_pub -h YOUR_BROKER_IP \
  -t envcube/kitchen/cmd/threshold \
  -m '{"sensor":"smoke_alert","value":1200}'
```

---

## Phase 1 sign-off → Phase 2

Once integration test checklist is complete and HA dashboard is live:

**Phase 2 scope:**
- Custom PCB design (ESP32-C6 + all sensors on one board)
- Native iOS/Android app replacing captive portal
- Cloud backend (heartbeat relay, remote alerts outside home WiFi)
- BLE provisioning via app
- Matter/Thread certification path (ESP32-C6 is already capable)
