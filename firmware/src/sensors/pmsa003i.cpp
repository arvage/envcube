// ============================================================
//  EnvCube — PMSA003I particulate matter driver
//
//  Notes:
//  - PMSA003I is a sealed module with internal laser + fan.
//    Connects via ZH-1.0mm 6-pin cable to pod PCB.
//    I²C address 0x12, bus voltage 3.3V compatible.
//
//  - Two PM readings available:
//    "Standard" (CF=1): calibrated for standard particle density
//    "Atmospheric": calibrated for real atmospheric conditions
//    We use atmospheric values — more accurate for indoor AQI.
//
//  - Fan noise: module produces ~1-2 dB(A) of fan noise.
//    Keep particulate pod away from noise pod to avoid
//    contaminating noise readings.
//
//  - Pod design: intake and exhaust vents on opposing faces
//    required for fan airflow. Module floats on foam isolators.
//
//  - Adafruit PM25AQI library handles the I²C protocol.
// ============================================================

#include "pmsa003i.h"
#include "../config.h"
#include <Wire.h>
#include <Adafruit_PM25AQI.h>

static Adafruit_PM25AQI _pm;
bool     Pmsa003i::_ready       = false;
uint32_t Pmsa003i::_lastReadMs  = 0;

// ── Pmsa003i::begin ──────────────────────────────────────────
bool Pmsa003i::begin() {
    // PMSA003I I²C mode (vs UART mode on other Plantower sensors)
    if (!_pm.begin_I2C()) {
        Serial.println("[PMSA003I] ERROR: sensor not found (0x12)");
        Serial.println("[PMSA003I] Check: ZH-1.0mm cable seated, VCC=3.3V");
        _ready = false;
        return false;
    }

    _ready = true;
    Serial.println("[PMSA003I] Initialised — particulate sensor ready");
    Serial.println("[PMSA003I] Fan warm-up: allow 30 s for stable readings");
    return true;
}

// ── Pmsa003i::read ───────────────────────────────────────────
bool Pmsa003i::read(SensorReadings& r) {
    if (!_ready) {
        r.pm_ok = false;
        return false;
    }

    if (millis() - _lastReadMs < READ_INTERVAL_MS) return true;

    PM25_AQI_Data data;
    if (!_pm.read(&data)) {
        // No new frame yet — not an error
        return true;
    }

    // Use atmospheric environment values (vs CF=1 standard)
    uint16_t pm1  = data.pm10_env;
    uint16_t pm25 = data.pm25_env;
    uint16_t pm10 = data.pm100_env;

    // Sanity bounds (µg/m³)
    if (pm25 > 1000) {
        Serial.printf("[PMSA003I] Out-of-range PM2.5: %u — skipping\n", pm25);
        r.pm_ok = false;
        return false;
    }

    r.pm1_0  = pm1;
    r.pm2_5  = pm25;
    r.pm10   = pm10;
    r.pm_ok  = true;
    _lastReadMs = millis();

    // WHO guidelines context
    const char* quality =
        pm25 <= 5  ? "WHO annual" :
        pm25 <= 12 ? "Good" :
        pm25 <= 35 ? "Moderate" :
        pm25 <= 75 ? "Unhealthy" : "Hazardous";

    Serial.printf("[PMSA003I] PM1=%u  PM2.5=%u  PM10=%u µg/m³  (%s)\n",
                  pm1, pm25, pm10, quality);
    return true;
}

bool Pmsa003i::isReady() { return _ready; }
