# Pod Design Guidelines

> These notes govern physical sensor placement, enclosure venting,
> and mechanical design for each pod. Follow carefully — sensor accuracy
> depends on enclosure design as much as the sensor itself.

---

## Pod connector spec (pogo pin + magnetic)

All pods share the same 6-contact pogo pin interface:

| Pin | Signal | Notes |
|---|---|---|
| 1 | VCC 3.3V | From hub regulator |
| 2 | GND | |
| 3 | SDA | I²C data (shared bus) |
| 4 | SCL | I²C clock (shared bus) |
| 5 | AUX | UART TX or I²S SCK or ADC (pod-specific) |
| 6 | 1-Wire ID | DS2401 chip in pod — hub auto-detects pod type |

Magnets: N52 neodymium 6×2mm, 2 per pod face — guide alignment only, not structural.

---

## Pod 1 — Thermal (SHT40 + BMP280)

**Goal:** True room-air temperature, isolated from hub electronics.

**Design rules:**
- Mount SHT40 at the outermost face of the pod, facing the room
- Slot vents on 3 sides (not bottom — avoid dust accumulation)
- Foam gasket between pod connector and hub face — blocks convective heat
- No internal walls that create dead-air pockets
- BMP280 can be anywhere — pressure is not affected by airflow

**Why this matters for sales:**  
_"True room temperature, not what your circuit board thinks it is"_

---

## Pod 2 — Smoke + CO₂ (MQ-2 + SCD41)

**Goal:** Fast smoke response, accurate CO₂ reading.

**Design rules:**
- **Large vents required** — smoke particles must enter the pod within seconds
  of a fire event. Minimum 40% open area on the intake face.
- MQ-2 heater element generates heat (~800mW). Place SCD41 on the
  opposite side of an internal divider to avoid temperature contamination.
- SCD41 sensor window must face the vent — it needs fresh air exchange.
- Pod face should be clearly labeled: "SMOKE · CO₂" in embossed text.
- **Do not seal this pod** — it must breathe freely.

**MQ-2 warm-up:**  
Firmware enforces 60 s warm-up on boot. During warm-up, smoke readings
are suppressed. LED pulses amber during this period.

---

## Pod 3 — Air Quality (SGP41 + VEML7700)

**Goal:** Representative room-air VOC + NOx reading + accurate lux.

**Design rules:**
- SGP41 requires airflow — 2–3 small vent slots (2mm wide) on the sensor face
- Mesh vent cover recommended (prevents insects/dust reaching sensor membrane)
- VEML7700 needs an optical window on the pod face — clear acrylic insert
  or a simple 5mm hole. The lux sensor should face the room, not internal
- SGP41 compensation: firmware reads SHT40 temperature + humidity over I²C
  and passes them to the SGP41 driver for humidity compensation

---

## Pod 4 — Presence (HLK-LD2410C)

**Goal:** Reliable static + motion detection through pod face.

**Design rules:**
- **CRITICAL:** No metal in front of the radar antenna.
  Pod face must be plastic (ABS or PC). Even metallic paint will block signal.
- Pod face thickness must be < 3mm for best radar performance.
- Radar antenna faces outward — do not mount module sideways.
- No vents needed (radar signal penetrates through solid plastic).
- Sensitivity tunable via UART — factory setting works for most rooms.

**Detection spec:** 5m max range, 0.75m resolution, detects still occupants.

---

## Pod 5 — Particulate (PMSA003I)

**Goal:** Accurate PM1/2.5/PM10 readings with adequate airflow through internal fan.

**Design rules:**
- This pod is physically larger than others (~50×50×20mm minimum)
- **Airflow path:** intake vent on one face → through module → exhaust opposite face
  Both faces must be open — the internal fan creates flow, not diffusion
- Mount module on soft foam isolators, not rigid — reduces vibration noise
- ZH-1.0mm 6-pin cable connects module to pod PCB (no soldering to module)
- Label pod: "PM1.0 / PM2.5 / PM10"

**Fan life:** ~3 years continuous operation. Document in product spec sheet.

---

## Pod 6 — Noise (ICS-43434)

**Goal:** Accurate room dB(A) readings — no mechanical resonance artifacts.

**Design rules:**
- Microphone port: single 2–3mm pinhole on pod face
  (larger openings create wind noise and airflow artifacts)
- Internal cavity between pinhole and mic capsule: ~5mm — acts as acoustic buffer
- Pod enclosure must be rigid — flexible enclosures resonate and add noise floor
- Keep pod away from the hub buzzer/speaker — feedback at close range
- Mic faces room, away from pod connector face
- Firmware uses 1-second Leq (energy-average) windows — not instantaneous peak

**Calibration:** ICS-43434 is factory calibrated ±1dB — no field calibration needed.
Firmware applies A-weighting filter and compensation curve per datasheet.
