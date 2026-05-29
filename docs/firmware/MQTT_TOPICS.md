# MQTT Topic Reference

All topics follow the pattern: `envcube/<room_name>/<sensor>`

Room name is set during provisioning (e.g. "kitchen", "bedroom").
It is lowercase and spaces replaced with hyphens.

---

## Sensor topics (published every ~10 s)

```
envcube/kitchen/temperature     {"value": 22.3,   "unit": "C",      "ok": true}
envcube/kitchen/humidity        {"value": 48.1,   "unit": "%",      "ok": true}
envcube/kitchen/pressure        {"value": 1013.2, "unit": "hPa",    "ok": true}
envcube/kitchen/co2             {"value": 612,    "unit": "ppm",    "ok": true}
envcube/kitchen/smoke_raw       {"value": 320,    "unit": "raw",    "ok": true}
envcube/kitchen/smoke           {"value": false,  "ok": true}
envcube/kitchen/voc_index       {"value": 45,     "ok": true}
envcube/kitchen/nox_index       {"value": 4,      "ok": true}
envcube/kitchen/pm1             {"value": 3,      "unit": "ug/m3",  "ok": true}
envcube/kitchen/pm25            {"value": 8,      "unit": "ug/m3",  "ok": true}
envcube/kitchen/pm10            {"value": 11,     "unit": "ug/m3",  "ok": true}
envcube/kitchen/noise_db        {"value": 42.1,   "unit": "dBA",    "ok": true}
envcube/kitchen/lux             {"value": 340,    "unit": "lx",     "ok": true}
envcube/kitchen/presence        {"value": true,   "distance_cm": 180, "ok": true}
```

## Alert + status topics

```
envcube/kitchen/alert           {"level": 0, "source": "none", "message": "All clear"}
envcube/kitchen/status          {"online": true, "rssi": -68, "ip": "192.168.1.42",
                                  "uptime_s": 14400, "version": "0.1.0"}
envcube/kitchen/heartbeat       {"ts": 1735000000, "cube_id": 42}
```

Alert levels: `0=all_clear  1=warning  2=alert  3=critical`

## Command topics (subscribe — receive commands from HA/app)

```
envcube/kitchen/cmd/threshold   {"sensor": "co2_warn", "value": 800}
envcube/kitchen/cmd/reboot      {}
envcube/kitchen/cmd/identify    {}   ← flashes LED for 5 s
envcube/kitchen/cmd/ota_check   {}
```

## Home Assistant auto-discovery

Published once on connect to:
```
homeassistant/sensor/envcube_kitchen_<name>/config
homeassistant/binary_sensor/envcube_kitchen_<name>/config
```

HA entities created automatically (no YAML needed):
- `sensor.envcube_kitchen_temperature`
- `sensor.envcube_kitchen_humidity`
- `sensor.envcube_kitchen_co2`
- `sensor.envcube_kitchen_voc_index`
- `sensor.envcube_kitchen_pm25`
- `sensor.envcube_kitchen_noise_db`
- `sensor.envcube_kitchen_lux`
- `binary_sensor.envcube_kitchen_smoke`
- `binary_sensor.envcube_kitchen_presence`
- `sensor.envcube_kitchen_alert_level`
