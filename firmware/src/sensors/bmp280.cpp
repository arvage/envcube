// ============================================================
//  EnvCube — BMP280 barometric pressure driver
//
//  Notes:
//  - BMP280 shares I²C bus with all other sensors (Wire)
//  - Temperature from BMP280 NOT used — SHT40 is more accurate
//    and already thermal-corrected. BMP280 temp is discarded.
//  - Oversampling: ×4 pressure, ×1 temp (minimum required)
//  - IIR filter coefficient 4 — reduces short-term fluctuations
// ============================================================

#include "bmp280.h"
#include "../config.h"
#include <Wire.h>
#include <Adafruit_BMP280.h>

static Adafruit_BMP280 _bmp(&Wire);
bool  Bmp280Driver::_ready         = false;
float Bmp280Driver::seaLevelPressure = 1013.25f;

// ── Bmp280Driver::begin ──────────────────────────────────────
bool Bmp280Driver::begin() {
    // I²C already started by SHT40::begin() — Wire.begin() is idempotent
    if (!_bmp.begin(I2C_ADDR_BMP280)) {
        Serial.println("[BMP280] ERROR: sensor not found (0x77)");
        Serial.println("[BMP280] Check: SDO pin HIGH for 0x77, LOW for 0x76");
        _ready = false;
        return false;
    }

    // Recommended settings for indoor navigation (Bosch datasheet table 4)
    _bmp.setSampling(
        Adafruit_BMP280::MODE_NORMAL,
        Adafruit_BMP280::SAMPLING_X1,    // temp oversampling
        Adafruit_BMP280::SAMPLING_X4,    // pressure oversampling
        Adafruit_BMP280::FILTER_X4,      // IIR filter
        Adafruit_BMP280::STANDBY_MS_125  // 125 ms standby
    );

    _ready = true;
    Serial.println("[BMP280] Initialised — pressure ×4, IIR ×4");
    return true;
}

// ── Bmp280Driver::read ───────────────────────────────────────
bool Bmp280Driver::read(SensorReadings& r) {
    if (!_ready) return false;

    float pressure = _bmp.readPressure() / 100.0f;  // Pa → hPa

    // Sanity bounds
    if (pressure < 800.0f || pressure > 1100.0f) {
        Serial.printf("[BMP280] Out-of-range pressure: %.1f hPa — skipping\n", pressure);
        return false;
    }

    r.pressure_hpa = pressure;
    r.pressure_ok  = true;

    Serial.printf("[BMP280] P=%.1f hPa\n", pressure);
    return true;
}

bool Bmp280Driver::isReady() { return _ready; }
