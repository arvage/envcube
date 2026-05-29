// ============================================================
//  EnvCube — VEML7700 ambient light driver
//
//  Notes:
//  - Auto-gain and auto-integration time selection enabled
//    to maximise accuracy across full 0.0036–120k lux range
//  - Adafruit library handles gain/integration auto-selection
//  - Lux used for: OLED display, day/night context in alerts,
//    presence suppression context (dark room = likely sleeping)
// ============================================================

#include "veml7700.h"
#include "../config.h"
#include <Wire.h>
#include <Adafruit_VEML7700.h>

static Adafruit_VEML7700 _veml;
bool Veml7700Driver::_ready = false;

// ── Veml7700Driver::begin ────────────────────────────────────
bool Veml7700Driver::begin() {
    if (!_veml.begin()) {
        Serial.println("[VEML7700] ERROR: sensor not found (0x10)");
        _ready = false;
        return false;
    }

    _veml.setGain(VEML7700_GAIN_1);
    _veml.setIntegrationTime(VEML7700_IT_100MS);

    // Auto-gain enabled — library adjusts on each read
    _veml.powerSaveEnable(false);

    _ready = true;
    Serial.println("[VEML7700] Initialised — auto-gain, 100ms integration");
    return true;
}

// ── Veml7700Driver::read ─────────────────────────────────────
bool Veml7700Driver::read(SensorReadings& r) {
    if (!_ready) return false;

    // getAutoLux() adjusts gain + integration automatically
    float lux = _veml.readLux(VEML_LUX_AUTO);

    if (lux < 0) {
        Serial.println("[VEML7700] Read error");
        return false;
    }

    r.lux = (uint32_t)lux;

    Serial.printf("[VEML7700] Lux=%.1f (%s)\n",
                  lux,
                  lux < THRESH_LUX_DARK ? "dark" :
                  lux < 100 ? "dim" :
                  lux < 500 ? "normal" : "bright");
    return true;
}

bool Veml7700Driver::isReady() { return _ready; }
