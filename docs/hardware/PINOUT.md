# ESP32-C6 Pin Assignment — EnvCube

## I²C Bus (shared — all pod sensors)

| ESP32-C6 Pin | Signal | Connected to |
|---|---|---|
| GPIO6 | SDA | SHT40, SCD41, SGP41, BMP280, VEML7700, PMSA003I, OLED |
| GPIO7 | SCL | (same as above) |

> 4.7kΩ pull-up resistors required on both SDA and SCL to 3.3V.

## UART0 — Presence sensor

| ESP32-C6 Pin | Signal | Connected to |
|---|---|---|
| GPIO16 (TX) | → LD2410C RX | HLK-LD2410C |
| GPIO17 (RX) | ← LD2410C TX | HLK-LD2410C |

Baud: 256000

## UART1 — DFPlayer Mini

| ESP32-C6 Pin | Signal | Connected to |
|---|---|---|
| GPIO4 (TX) | → DFPlayer RX | DFPlayer Mini |
| GPIO5 (RX) | ← DFPlayer TX | DFPlayer Mini |

Baud: 9600

## I²S — Noise microphone

| ESP32-C6 Pin | Signal | Connected to |
|---|---|---|
| GPIO10 | SCK (bit clock) | ICS-43434 |
| GPIO11 | WS (word select) | ICS-43434 |
| GPIO12 | SD (data in) | ICS-43434 |

## Analog — Smoke sensor

| ESP32-C6 Pin | Signal | Connected to |
|---|---|---|
| GPIO0 | ADC1 CH0 | MQ-2 analog output |

> GPIO0 is the only reliable ADC pin on ESP32-C6 when WiFi is active.
> Do not use GPIO1–5 for ADC while WiFi is running.

## Digital outputs

| ESP32-C6 Pin | Signal | Connected to |
|---|---|---|
| GPIO8 | Data | WS2812B RGB LED |
| GPIO9 | PWM | Passive piezo buzzer |

## User interface

| ESP32-C6 Pin | Signal | Connected to |
|---|---|---|
| GPIO18 | Button input | Tactile button → GND (INPUT_PULLUP) |

## USB (built-in)

| Signal | Notes |
|---|---|
| USB D+ / D- | ESP32-C6 native USB CDC for serial monitor + flashing |

## Free GPIO (for future use)

GPIO2, GPIO3, GPIO13, GPIO14, GPIO15, GPIO19, GPIO20, GPIO21, GPIO22, GPIO23

## I²C Address Map

| Address | Device |
|---|---|
| 0x3C | SSD1306 OLED |
| 0x10 | VEML7700 ambient light |
| 0x12 | PMSA003I particulate |
| 0x44 | SHT40 temp/humidity |
| 0x59 | SGP41 VOC/NOx |
| 0x62 | SCD41 CO₂ |
| 0x77 | BMP280 pressure |

> All 7 addresses are unique — no conflicts on shared I²C bus.
