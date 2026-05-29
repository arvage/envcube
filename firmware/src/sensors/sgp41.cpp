// ============================================================
//  EnvCube — SGP41 VOC + NOx index driver
//  Fixed for actual SensirionI2CSgp41 library API:
//  - executeConditioning(rhTicks, tTicks, srawVoc&)  — 3 args, ref
//  - measureRawSignals(rhTicks, tTicks, srawVoc&, srawNox&) — refs
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

    // Start conditioning — library takes (rhTicks, tTicks, srawVoc&)
    // Use default compensation: 50%RH, 25°C
    uint16_t srawVoc = 0;
    uint16_t err = _sgp.executeConditioning(0x8000, 0x6666, srawVoc);
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

    if (millis() - _lastReadMs < READ_INTERVAL_MS) return true;
    _lastReadMs = millis();

    // Convert T/H to SGP41 ticks
    float    t       = (temperature_c > -40.0f) ? temperature_c : 25.0f;
    float    h       = (humidity_rh   >   0.0f) ? humidity_rh   : 50.0f;
    uint16_t t_ticks = (uint16_t)((t + 45.0f) / 175.0f * 65535.0f);
    uint16_t h_ticks = (uint16_t)(h / 100.0f * 65535.0f);

    uint16_t srawVoc = 0, srawNox = 0;
    uint16_t err;

    if (_conditioning && (millis() - _conditioningStartMs < CONDITIONING_MS)) {
        // Still conditioning — executeConditioning returns srawVoc by ref
        err = _sgp.executeConditioning(h_ticks, t_ticks, srawVoc);
        if (err) {
            Serial.printf("[SGP41] Conditioning read error: %u\n", err);
        }
        r.aq_ok = false;
        uint32_t remaining = (CONDITIONING_MS - (millis() - _conditioningStartMs)) / 1000;
        Serial.printf("[SGP41] Conditioning... %lu s remaining\n", remaining);
        return true;
    } else {
        _conditioning = false;
    }

    // Normal measurement — library uses references not pointers
    err = _sgp.measureRawSignals(h_ticks, t_ticks, srawVoc, srawNox);
    if (err) {
        Serial.printf("[SGP41] Measurement error: %u\n", err);
        r.aq_ok = false;
        return false;
    }

    int32_t vocIndex = _vocAlgorithm.process(srawVoc);
    int32_t noxIndex = _noxAlgorithm.process(srawNox);

    r.voc_index = (int16_t)constrain(vocIndex, 0, 500);
    r.nox_index = (int16_t)constrain(noxIndex, 1, 500);
    r.aq_ok     = true;

    Serial.printf("[SGP41] VOC=%d  NOx=%d  (raw: VOC=%u NOx=%u)\n",
                  vocIndex, noxIndex, srawVoc, srawNox);
    return true;
}

bool Sgp41::isReady() { return _ready; }