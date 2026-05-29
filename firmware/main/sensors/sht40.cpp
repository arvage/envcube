// ============================================================
//  EnvCube — SHT40 temperature + humidity driver
//
//  Notes:
//  - Adafruit SHT4x library handles CRC checking internally
//  - High precision mode (measureHighPrecision) used — 8.3ms
//  - Thermal pod is physically separated from hub to avoid
//    ESP32-C6 self-heating contaminating readings
//  - Software offset correction applied using ESP32 internal
//    temperature sensor for residual drift compensation
// ============================================================

#include "sht40.h"
#include "../config.h"
#include <Wire.h>
#include <Adafruit_SHT4x.h>

static Adafruit_SHT4x _sht;
bool Sht40::_ready = false;

// ── Sht40::begin ─────────────────────────────────────────────
bool Sht40::begin() {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    if (!_sht.begin()) {
        Serial.println("[SHT40] ERROR: sensor not found on I²C bus (0x44)");
        Serial.println("[SHT40] Check wiring: SDA→GPIO6, SCL→GPIO7, 4.7kΩ pull-ups");
        _ready = false;
        return false;
    }

    // High precision: best accuracy (8.3 ms measurement time)
    _sht.setPrecision(SHT4X_HIGH_PRECISION);
    // No heater — heater biases humidity readings
    _sht.setHeater(SHT4X_NO_HEATER);

    _ready = true;
    Serial.println("[SHT40] Initialised — high precision, no heater");
    return true;
}

// ── Sht40::read ──────────────────────────────────────────────
bool Sht40::read(SensorReadings& r) {
    if (!_ready) {
        r.thermal_ok = false;
        return false;
    }

    sensors_event_t temp_event, hum_event;
    if (!_sht.getEvent(&hum_event, &temp_event)) {
        Serial.println("[SHT40] Read error");
        r.thermal_ok = false;
        return false;
    }

    float raw_temp = temp_event.temperature;
    float raw_hum  = hum_event.relative_humidity;

    // ── Thermal offset correction ─────────────────────────────
    // ESP32-C6 internal sensor gives ~chip junction temp.
    // Empirical: offset ≈ (internal_temp - 25) × 0.35
    // This factor should be calibrated against a reference
    // thermometer in your prototype environment.
    float internal_temp = temperatureRead();  // ESP32 built-in
    float correction    = (internal_temp - 25.0f) * 0.35f;
    float corrected_temp = raw_temp - correction;

    // Sanity bounds — reject readings outside physical range
    if (corrected_temp < -40.0f || corrected_temp > 85.0f ||
        raw_hum < 0.0f          || raw_hum > 100.0f) {
        Serial.printf("[SHT40] Out-of-range reading: T=%.1f H=%.1f — skipping\n",
                      corrected_temp, raw_hum);
        r.thermal_ok = false;
        return false;
    }

    r.temperature_c = corrected_temp;
    r.humidity_rh   = raw_hum;
    r.thermal_ok    = true;

    Serial.printf("[SHT40] T=%.2f°C (raw=%.2f, offset=%.2f)  RH=%.1f%%\n",
                  corrected_temp, raw_temp, correction, raw_hum);
    return true;
}

bool Sht40::isReady() { return _ready; }
