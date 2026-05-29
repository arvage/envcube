// ============================================================
//  EnvCube — SCD41 CO₂ sensor driver
//
//  Notes:
//  - SCD41 uses NDIR (non-dispersive infrared) — true CO₂ detection
//  - Start-up time: ~1 second after power-on before first measurement
//  - Periodic measurement mode: one reading every 5 seconds
//  - Automatic Self-Calibration (ASC) enabled by default — assumes
//    sensor sees ~420ppm (outdoor air) at least once every 7 days
//  - Temperature + humidity compensation: uses SHT40 readings
//    for best CO₂ accuracy (SCD41 has internal T/H sensor but
//    hub heat would bias it — SHT40 in pod is more accurate)
//  - Do NOT place SCD41 near MQ-2 heater element inside pod
// ============================================================

#include "scd41.h"
#include "../config.h"
#include <Wire.h>
#include <SensirionI2CScd4x.h>

static SensirionI2CScd4x _scd;
bool     Scd41::_ready           = false;
uint32_t Scd41::_lastMeasureMs   = 0;

// ── Scd41::begin ─────────────────────────────────────────────
bool Scd41::begin() {
    _scd.begin(Wire);

    // Stop any previously running measurement (robustness on reboot)
    uint16_t err = _scd.stopPeriodicMeasurement();
    delay(500);  // Required after stop command

    // Apply temperature offset to compensate for pod self-heating
    // Default offset from Sensirion: 4°C. Our pod runs cooler
    // than a sealed enclosure so we reduce this slightly.
    // Fine-tune against SHT40 during calibration.
    err = _scd.setTemperatureOffset(3.5f);
    if (err) {
        Serial.printf("[SCD41] WARNING: setTemperatureOffset failed (err=%u)\n", err);
    }

    // Start periodic measurement (one reading every 5 s)
    err = _scd.startPeriodicMeasurement();
    if (err) {
        Serial.printf("[SCD41] ERROR: startPeriodicMeasurement failed (err=%u)\n", err);
        _ready = false;
        return false;
    }

    _ready = true;
    _lastMeasureMs = millis();
    Serial.println("[SCD41] Initialised — periodic measurement started");
    Serial.println("[SCD41] First reading available in ~5 seconds");
    return true;
}

// ── Scd41::read ──────────────────────────────────────────────
bool Scd41::read(SensorReadings& r) {
    if (!_ready) {
        r.co2_ok = false;
        return false;
    }

    // Respect 5-second minimum interval
    if (millis() - _lastMeasureMs < MEASURE_INTERVAL_MS) {
        return true;  // Not an error — just not time yet
    }

    bool dataReady = false;
    uint16_t err = _scd.getDataReadyFlag(dataReady);
    if (err || !dataReady) {
        // Not ready yet — not an error
        return true;
    }

    uint16_t co2     = 0;
    float    temp    = 0.0f;
    float    hum     = 0.0f;

    err = _scd.readMeasurement(co2, temp, hum);
    if (err) {
        Serial.printf("[SCD41] Read error: %u\n", err);
        r.co2_ok = false;
        return false;
    }

    // co2 == 0 means measurement not yet valid (first reading after boot)
    if (co2 == 0) {
        Serial.println("[SCD41] Measurement not valid yet (warming up)");
        r.co2_ok = false;
        return true;
    }

    // Sanity bounds — CO₂ range 400–40000 ppm
    if (co2 < 400 || co2 > 40000) {
        Serial.printf("[SCD41] Out-of-range CO₂: %u ppm — skipping\n", co2);
        r.co2_ok = false;
        return false;
    }

    r.co2_ppm = co2;
    r.co2_ok  = true;
    _lastMeasureMs = millis();

    Serial.printf("[SCD41] CO₂=%u ppm  T=%.1f°C  RH=%.1f%%\n", co2, temp, hum);
    return true;
}

// ── Scd41::forceCalibration ──────────────────────────────────
bool Scd41::forceCalibration(uint16_t target_ppm) {
    Serial.printf("[SCD41] Force calibration to %u ppm...\n", target_ppm);

    uint16_t err = _scd.stopPeriodicMeasurement();
    delay(500);

    uint16_t correction = 0;
    err = _scd.performForcedRecalibration(target_ppm, correction);
    if (err) {
        Serial.printf("[SCD41] Calibration failed (err=%u)\n", err);
        _scd.startPeriodicMeasurement();
        return false;
    }

    Serial.printf("[SCD41] Calibration complete — correction=%d ppm\n",
                  (int16_t)correction);
    _scd.startPeriodicMeasurement();
    return true;
}

bool Scd41::isReady() { return _ready; }
