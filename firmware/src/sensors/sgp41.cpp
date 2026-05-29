// ============================================================
//  EnvCube — SGP41 VOC + NOx index driver
//
//  Notes:
//  - SGP41 outputs Gas Index Algorithm (GIA) values, not raw
//    resistance. Algorithm runs on-chip with continuous baseline
//    learning — improves accuracy over hours/days of operation.
//  - Conditioning phase: first 10 s uses executeConditioning()
//    which warms the metal oxide sensing elements
//  - Humidity compensation: passing SHT40 readings significantly
//    improves VOC index accuracy (uncompensated = 50%RH assumed)
//  - Default compensation if SHT40 not ready: 25°C / 50%RH
//  - measureRawSignals() is used in production (vs conditioning)
// ============================================================

#include "sgp41.h"
#include "../config.h"
#include <Wire.h>
#include <SensirionI2CSgp41.h>
#include <NOxGasIndexAlgorithm.h>
#include <VOCGasIndexAlgorithm.h>

static SensirionI2CSgp41   _sgp;
static VOCGasIndexAlgorithm _vocAlgorithm;
static NOxGasIndexAlgorithm _noxAlgorithm;

bool     Sgp41::_ready               = false;
bool     Sgp41::_conditioning        = true;
uint32_t Sgp41::_conditioningStartMs = 0;
uint32_t Sgp41::_lastReadMs          = 0;

// ── Sgp41::begin ─────────────────────────────────────────────
bool Sgp41::begin() {
    _sgp.begin(Wire);

    uint16_t err = _sgp.executeConditioning(0x8000, 0x6666, nullptr, nullptr);
    // Note: conditioning errors are non-fatal — sensor may already be warm
    if (err) {
        Serial.printf("[SGP41] WARNING: conditioning start err=%u (non-fatal)\n", err);
    }

    _conditioning        = true;
    _conditioningStartMs = millis();
    _ready               = true;

    Serial.println("[SGP41] Initialised — conditioning 10 s...");
    return true;
}

// ── Sgp41::read ──────────────────────────────────────────────
bool Sgp41::read(SensorReadings& r, float temperature_c, float humidity_rh) {
    if (!_ready) {
        r.aq_ok = false;
        return false;
    }

    // Rate-limit
    if (millis() - _lastReadMs < READ_INTERVAL_MS) return true;
    _lastReadMs = millis();

    // Convert T/H to SGP41 ticks format
    // Formula: (T + 45) / 175 * 65535  and  RH / 100 * 65535
    float    t   = temperature_c > 0 ? temperature_c : 25.0f;
    float    h   = humidity_rh   > 0 ? humidity_rh   : 50.0f;
    uint16_t t_ticks = (uint16_t)((t + 45.0f) / 175.0f * 65535.0f);
    uint16_t h_ticks = (uint16_t)(h / 100.0f * 65535.0f);

    uint16_t srawVoc = 0, srawNox = 0;
    uint16_t err;

    if (_conditioning && (millis() - _conditioningStartMs < CONDITIONING_MS)) {
        // Still in conditioning phase — run conditioning command
        err = _sgp.executeConditioning(t_ticks, h_ticks, &srawVoc, nullptr);
        if (err) {
            Serial.printf("[SGP41] Conditioning read error: %u\n", err);
        }
        r.aq_ok = false;  // Values not valid during conditioning
        Serial.printf("[SGP41] Conditioning... %lu s remaining\n",
                      (CONDITIONING_MS - (millis() - _conditioningStartMs)) / 1000);
        return true;
    } else {
        _conditioning = false;
    }

    // Normal measurement
    err = _sgp.measureRawSignals(t_ticks, h_ticks, &srawVoc, &srawNox);
    if (err) {
        Serial.printf("[SGP41] Measurement error: %u\n", err);
        r.aq_ok = false;
        return false;
    }

    // Process through GIA algorithm
    int32_t vocIndex = _vocAlgorithm.process(srawVoc);
    int32_t noxIndex = _noxAlgorithm.process(srawNox);

    // Clamp to valid range
    vocIndex = constrain(vocIndex, 0, 500);
    noxIndex = constrain(noxIndex, 1, 500);

    r.voc_index = (int16_t)vocIndex;
    r.nox_index = (int16_t)noxIndex;
    r.aq_ok     = true;

    Serial.printf("[SGP41] VOC=%d  NOx=%d  (raw: VOC=%u NOx=%u)\n",
                  vocIndex, noxIndex, srawVoc, srawNox);
    return true;
}

bool Sgp41::isReady() { return _ready; }
