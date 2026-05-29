// ============================================================
//  EnvCube — MQ-2 smoke sensor driver
//
//  Notes:
//  - MQ-2 uses a heated metal-oxide sensing element.
//    Heater current: ~180mA at 5V. Power from USB rail, not 3.3V.
//    Analog output (AO pin) → ESP32-C6 GPIO0 ADC input.
//    Digital output (DO pin) is not used — we read raw ADC.
//
//  - ADC on ESP32-C6:
//    Only GPIO0 (ADC1_CH0) is used while WiFi is active.
//    Other ADC1 channels are fine; ADC2 unavailable with WiFi.
//    11dB attenuation → full 0–3.3V range → 12-bit (0–4095)
//
//  - Warm-up: MQ-2 needs 60 seconds after power-on for readings
//    to stabilise. Firmware enforces this delay in taskSensors.
//    Readings during warm-up are not pushed to alert engine.
//
//  - Calibration: baseline_clean_air should be measured in
//    fresh outdoor air or a very well-ventilated room.
//    Typical clean-air raw value: 300–600 (varies by module).
//    Smoke threshold ratio Rs/Ro ≈ 3 in clean air.
//
//  - Multi-sampling: 10 ADC reads averaged per call to reduce
//    noise (ESP32 ADC has non-linearity near rail voltages).
// ============================================================

#include "mq2.h"
#include "../storage/nvs_config.h"
#include "../config.h"
#include <Arduino.h>

bool     Mq2::_ready       = false;
bool     Mq2::_warmedUp    = false;
uint32_t Mq2::_bootTimeMs  = 0;
uint16_t Mq2::baseline_clean_air = 400;  // Default — calibrate in clean air

// ── Mq2::begin ───────────────────────────────────────────────
bool Mq2::begin() {
    analogSetPinAttenuation(PIN_MQ2_AO, ADC_11db);
    analogReadResolution(12);  // 12-bit → 0–4095

    _bootTimeMs = millis();
    _ready      = true;
    _warmedUp   = false;

    Serial.printf("[MQ-2] Initialised — warming up for %u seconds...\n",
                  MQ2_WARMUP_MS / 1000);
    Serial.println("[MQ-2] CALIBRATION: Run Mq2::calibrateCleanAir() in fresh air");
    Serial.printf("[MQ-2] Current baseline: %u (raw ADC)\n", baseline_clean_air);
    return true;
}

// ── Mq2::read ────────────────────────────────────────────────
bool Mq2::read(SensorReadings& r) {
    if (!_ready) {
        r.smoke_ok = false;
        return false;
    }

    // Enforce warm-up period
    if (!_warmedUp) {
        if (millis() - _bootTimeMs < MQ2_WARMUP_MS) {
            r.smoke_ok = false;
            return true;  // Not an error — still warming
        }
        _warmedUp = true;
        Serial.println("[MQ-2] Warm-up complete — readings now active");
    }

    // Multi-sample average to reduce ADC noise
    uint32_t sum = 0;
    for (uint8_t i = 0; i < ADC_SAMPLES; i++) {
        sum += analogRead(PIN_MQ2_AO);
        delayMicroseconds(500);
    }
    uint16_t raw = (uint16_t)(sum / ADC_SAMPLES);

    // Sanity: raw should be above baseline in clean air
    // Values near 0 usually mean sensor not connected
    if (raw < 50) {
        Serial.println("[MQ-2] WARNING: very low ADC reading — check wiring");
        r.smoke_ok = false;
        return false;
    }

    r.smoke_raw = raw;
    r.smoke_ok  = true;

    // Log with context
    const char* level = "clear";
    if      (raw >= g_config.thresh_smoke_alert) level = "ALERT";
    else if (raw >= g_config.thresh_smoke_warn)  level = "WARN";

    Serial.printf("[MQ-2] Raw=%u  baseline=%u  status=%s\n",
                  raw, baseline_clean_air, level);
    return true;
}

// ── Mq2::calibrateCleanAir ───────────────────────────────────
uint16_t Mq2::calibrateCleanAir(uint8_t samples) {
    Serial.println("[MQ-2] Calibrating clean-air baseline...");
    Serial.println("[MQ-2] Ensure sensor is in fresh outdoor air or very well ventilated room");

    uint32_t sum = 0;
    for (uint8_t i = 0; i < samples; i++) {
        sum += analogRead(PIN_MQ2_AO);
        delay(200);
        Serial.print(".");
    }
    Serial.println();

    uint16_t baseline = (uint16_t)(sum / samples);
    baseline_clean_air = baseline;

    Serial.printf("[MQ-2] Calibration complete — clean air baseline: %u\n", baseline);
    Serial.println("[MQ-2] Save this value as Mq2::baseline_clean_air in mq2.cpp");
    return baseline;
}

bool Mq2::isReady()    { return _ready; }
bool Mq2::isWarmedUp() { return _warmedUp; }