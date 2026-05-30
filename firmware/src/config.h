#pragma once
// ============================================================
//  EnvCube — Central configuration
//  Edit this file to change pins, thresholds, or behaviour.
// ============================================================

// ── Firmware version ────────────────────────────────────────
#define ENVCUBE_VERSION       "0.5.0"
#define ENVCUBE_BUILD_PHASE   1

// ── I²C bus ─────────────────────────────────────────────────
// All I²C sensors share one bus (SDA/SCL).
// 4.7kΩ pull-ups required on both lines.
#define PIN_I2C_SDA           6
#define PIN_I2C_SCL           7

// ── I²C addresses ───────────────────────────────────────────
#define I2C_ADDR_SHT40        0x44
#define I2C_ADDR_SCD41        0x62
#define I2C_ADDR_SGP41        0x59
#define I2C_ADDR_BMP280       0x76
#define I2C_ADDR_VEML7700     0x10
#define I2C_ADDR_PMSA003I     0x12
#define I2C_ADDR_OLED         0x3C

// ── UART — LD2410C presence sensor ──────────────────────────
#define PIN_LD2410_TX         16    // ESP → LD2410C RX
#define PIN_LD2410_RX         17    // LD2410C TX → ESP
#define LD2410_BAUD           256000

// ── UART — DFPlayer Mini speaker ────────────────────────────
#define PIN_DFPLAYER_TX       4     // ESP → DFPlayer RX
#define PIN_DFPLAYER_RX       5     // DFPlayer TX → ESP
#define DFPLAYER_BAUD         9600

// ── I²S — ICS-43434 MEMS microphone ─────────────────────────
#define PIN_I2S_SCK           10    // Bit clock
#define PIN_I2S_WS            11    // Word select (L/R)
#define PIN_I2S_SD            12    // Serial data in

// ── Analog — MQ-2 smoke sensor ──────────────────────────────
#define PIN_MQ2_AO            0     // ADC1 channel 0
#define MQ2_WARMUP_MS         60000 // 60 s warm-up on boot

// ── Outputs ─────────────────────────────────────────────────
#define PIN_LED_DATA          8     // WS2812B data
#define PIN_BUZZER            9     // Passive piezo (PWM)
#define PIN_BUTTON            18    // User button (INPUT_PULLUP)

// ── LED colours (WS2812B) ────────────────────────────────────
#define LED_COLOR_GREEN       CRGB(0,   180, 60)
#define LED_COLOR_AMBER       CRGB(255, 140,  0)
#define LED_COLOR_RED         CRGB(220,   0,  0)
#define LED_COLOR_BLUE        CRGB(0,    80, 220)  // provisioning
#define LED_COLOR_WHITE       CRGB(200, 200, 200)  // OTA
#define LED_BRIGHTNESS        80    // 0–255, ~30% to reduce heat

// ── Button timing ────────────────────────────────────────────
#define BTN_HOLD_PROVISION_MS   3000   // Hold 3 s → re-provision
#define BTN_HOLD_RESET_MS      10000   // Hold 10 s → factory reset
#define BTN_DOUBLE_TAP_MS        400   // Double-tap → cycle OLED screen

// ── WiFi + networking ────────────────────────────────────────
#define WIFI_AP_SSID          "EnvCube-Setup"
#define WIFI_AP_PASSWORD      ""          // Open AP for captive portal
#define WIFI_CONNECT_TIMEOUT  20          // seconds
#define WIFI_RETRY_INTERVAL   30000       // ms between reconnect attempts
#define MDNS_HOSTNAME         "envcube"   // resolves as envcube.local

// ── OTA ──────────────────────────────────────────────────────
#define OTA_PORT              3232
#define OTA_PASSWORD          "envcube-ota"  // change before production!

// ── MQTT ─────────────────────────────────────────────────────
#define MQTT_PORT             1883
#define MQTT_KEEPALIVE        60
#define MQTT_RECONNECT_MS     5000
// Topic root: envcube/<room_name>/...  (room_name set at provision time)
#define MQTT_TOPIC_ROOT       "envcube"
#define MQTT_HA_DISCOVERY     "homeassistant"

// ── Heartbeat (cloud watchdog) ────────────────────────────────
#define HEARTBEAT_INTERVAL_MS 60000   // 60 s between pings

// ── Weather (Open-Meteo) ──────────────────────────────────────
#define WEATHER_UPDATE_MS     600000  // 10 min
// Lat/lon stored in NVS, set during provisioning

// ── Sensor polling ────────────────────────────────────────────
#define POLL_FAST_MS          5000    // SHT40, MQ2, LD2410, PMSA003I
#define POLL_SLOW_MS          30000   // SCD41, SGP41 (slower sensors)
#define NOISE_WINDOW_MS       1000    // Leq integration window

// ── Alert thresholds (defaults, overridable via NVS) ──────────
// Temperature (°C)
#define THRESH_TEMP_WARN      28.0f
#define THRESH_TEMP_ALERT     35.0f

// Humidity (%RH)
#define THRESH_HUM_LOW_WARN   25.0f
#define THRESH_HUM_HIGH_WARN  70.0f
#define THRESH_HUM_ALERT      80.0f

// CO₂ (ppm)
#define THRESH_CO2_WARN       1000
#define THRESH_CO2_ALERT      2000
#define THRESH_CO2_CRITICAL   5000   // buzzer + voice

// VOC index (SGP41, 0–500, lower = better)
#define THRESH_VOC_WARN       150
#define THRESH_VOC_ALERT      250

// NOx index (SGP41, 0–500)
#define THRESH_NOX_WARN       20
#define THRESH_NOX_ALERT      50

// Smoke (MQ-2 ADC raw, 12-bit = 0–4095)
// Calibrate these values with known smoke concentrations
#define THRESH_SMOKE_WARN     800
#define THRESH_SMOKE_ALERT    1500   // buzzer + voice IMMEDIATELY

// Particulate PM2.5 (µg/m³ — WHO guidelines)
#define THRESH_PM25_WARN      12     // WHO annual guideline
#define THRESH_PM25_ALERT     35     // WHO 24h guideline
#define THRESH_PM25_CRITICAL  75

// Noise dB(A)
#define THRESH_NOISE_WARN     65     // Sustained conversation level
#define THRESH_NOISE_ALERT    80     // Prolonged exposure concern
#define THRESH_NOISE_CRITICAL 90     // OSHA action level

// Lux (VEML7700)
#define THRESH_LUX_DARK       10     // Below this = night/dark room

// ── ESP-NOW mesh ──────────────────────────────────────────────
#define ESPNOW_CHANNEL        1      // Must match router channel
#define ESPNOW_MAX_PEERS      10     // Max other cubes
// PMK (shared key for AES-128) — MUST be changed to a private key
// 16 bytes, printable ASCII
#define ESPNOW_PMK            "EnvCube-PMK-v1!!"
// Alert repeat count for reliability (broadcast sent N times)
#define ESPNOW_ALERT_REPEATS  3

// ── DFPlayer audio track mapping ─────────────────────────────
// Files on microSD: /01.mp3, /02.mp3, etc.
#define AUDIO_SMOKE           1
#define AUDIO_CO2_HIGH        2
#define AUDIO_CO2_CRITICAL    3
#define AUDIO_TEMP_HIGH       4
#define AUDIO_HUMIDITY_HIGH   5
#define AUDIO_AIR_QUALITY     6
#define AUDIO_PARTICULATE     7
#define AUDIO_NOISE_HIGH      8
#define AUDIO_ALL_CLEAR       9
#define AUDIO_WIFI_CONNECTED  10
#define AUDIO_PROVISIONING    11
#define AUDIO_VOLUME          20    // 0–30

// ── OLED display ──────────────────────────────────────────────
#define OLED_WIDTH            128
#define OLED_HEIGHT           64
#define OLED_SCREEN_TIMEOUT   30000  // ms before dimming
#define OLED_SCREENS_COUNT    3      // offline: sensors, status, info
