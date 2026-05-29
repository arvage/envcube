# Bill of Materials — EnvCube Prototype

> Buy 2× of everything to test ESP-NOW mesh from day one.

## Core Hub

| # | Component | Part / SKU | Supplier | Qty | ~Unit | Notes |
|---|---|---|---|---|---|---|
| 1 | ESP32-C6-DevKitC-1 | Adafruit #5672 | Adafruit | 2 | $10 | WiFi6 + Thread + ESP-NOW |
| 2 | SSD1306 OLED 0.96" 128×64 | Adafruit #326 | Adafruit | 2 | $7 | I²C 0x3C |
| 3 | WS2812B RGB LED (5mm) | Adafruit #1938 | Adafruit | 5 | $1 | Single addressable |
| 4 | DFPlayer Mini MP3 module | — | Amazon | 2 | $3 | UART speaker driver |
| 5 | 28mm 8Ω 1W speaker | — | Amazon | 2 | $2 | For DFPlayer |
| 6 | MicroSD card 2–8GB | — | Amazon | 2 | $6 | FAT32, for voice clips |
| 7 | Passive piezo buzzer 5V | Adafruit #160 | Adafruit | 4 | $1.50 | PWM driven |
| 8 | USB-C 5V 2A wall adapter | — | Amazon | 2 | $10 | Mains power |
| 9 | LiPo 3.7V 500mAh + charger | Adafruit #1570 | Adafruit | 2 | $8 | Backup power |

## Pod 1 — Thermal

| # | Component | Part / SKU | Supplier | Qty | ~Unit | Notes |
|---|---|---|---|---|---|---|
| 10 | Sensirion SHT40 breakout | Adafruit #5776 | Adafruit | 2 | $6 | ±0.2°C ±1.8%RH |
| 11 | Bosch BMP280 breakout | Adafruit #2651 | Adafruit | 2 | $10 | Pressure + backup temp |

## Pod 2 — Smoke + CO₂

| # | Component | Part / SKU | Supplier | Qty | ~Unit | Notes |
|---|---|---|---|---|---|---|
| 12 | Sensirion SCD41 breakout | Adafruit #5190 | Adafruit | 2 | $45 | True NDIR CO₂ ±40ppm |
| 13 | MQ-2 smoke sensor module | — | Amazon | 4 | $2.50 | Analog, 60 s warm-up |

## Pod 3 — Air Quality

| # | Component | Part / SKU | Supplier | Qty | ~Unit | Notes |
|---|---|---|---|---|---|---|
| 14 | Sensirion SGP41 breakout | Adafruit #6455 | Adafruit | 2 | $17 | VOC + NOx index |
| 15 | Vishay VEML7700 breakout | Adafruit #4162 | Adafruit | 2 | $5 | Ambient lux |

## Pod 4 — Presence

| # | Component | Part / SKU | Supplier | Qty | ~Unit | Notes |
|---|---|---|---|---|---|---|
| 16 | HLK-LD2410C mmWave radar | Hi-Link / Amazon | Amazon | 2 | $5 | 24GHz FMCW, UART |

## Pod 5 — Particulate Matter

| # | Component | Part / SKU | Supplier | Qty | ~Unit | Notes |
|---|---|---|---|---|---|---|
| 17 | Plantower PMSA003I | Adafruit #4632 | Adafruit | 2 | $45 | PM1/2.5/10, I²C |

## Pod 6 — Noise

| # | Component | Part / SKU | Supplier | Qty | ~Unit | Notes |
|---|---|---|---|---|---|---|
| 18 | TDK ICS-43434 MEMS mic | Adafruit #6049 | Adafruit | 2 | $6 | I²S, ±1dB factory cal |

## Essentials / Lab

| # | Component | Supplier | Qty | ~Cost | Notes |
|---|---|---|---|---|---|
| 19 | Large breadboard (830pt) | Amazon | 2 | $8 | One per cube |
| 20 | Dupont jumper wire kit | Amazon | 1 | $8 | M-M, M-F, F-F |
| 21 | 4.7kΩ resistor (pull-ups) | Amazon | 20 | $1 bag | I²C SDA + SCL pull-ups |
| 22 | USB logic analyzer (FX2LP) | Amazon | 1 | $12 | Debug I²C/UART/I²S |

---

## Total estimate

| Category | Cost |
|---|---|
| Core hub × 2 | ~$90 |
| All 6 pods × 2 | ~$190 |
| Lab essentials | ~$30 |
| **Total** | **~$310–340** |

> SCD41 ($45 ea) and PMSA003I ($45 ea) are the biggest line items.
> Order SCD41 first — stock fluctuates on Adafruit.

---

## Production part numbers (for PCB design)

| Sensor | JLCPCB Part# | ~@1k |
|---|---|---|
| SHT40-AD1B-R2 | C2909890 | $1.14 |
| SCD41-D-R2 | C3659294 | $16.50 |
| SGP41-D-R4 | C3659325 | $5.50 |
| BMP280 | C83291 | $0.55 |
| VEML7700-TR | C504893 | $0.60 |
| PMSA003I | Manual assembly | $8–12 |
| HLK-LD2410C-P | C19723500 | $3–5 |
| ICS-43434 | C5656610 | $2.50 |
