// ============================================================
//  EnvCube — ICS-43434 MEMS microphone noise level driver
//  Updated for Arduino ESP32 Core 3.x new I2S API
//
//  Arduino ESP32 Core 3.x uses ESP_I2S (I2SClass) which
//  wraps the ESP-IDF v5 I2S driver. The old driver/i2s.h
//  API with i2s_driver_install() is no longer available.
//
//  ICS-43434 wiring:
//    SCK  → GPIO10 (bit clock)
//    WS   → GPIO11 (word select / L-R clock)
//    SD   → GPIO12 (serial data)
//    L/R  → GND   (LEFT channel)
//    VDD  → 3.3V
//    GND  → GND
// ============================================================

#include "ics43434.h"
#include "../config.h"
#include <ESP_I2S.h>
#include <math.h>

bool  Ics43434::_ready         = false;
float Ics43434::_peakDb        = 0.0f;
float Ics43434::sensitivityOffset = 0.0f;

// A-weighting SOS filter coefficients for 44100 Hz
// Combined ICS-43434 equalization + A-weighting
static const float A_WEIGHT_SOS[][5] = {
    { 0.47732642f,  0.46294358f,  0.11224797f,  0.06681948f,  0.00111522f },
    { 1.0f,        -1.98905930f,  0.98908925f, -1.99755330f,  0.99755484f },
};
static const uint8_t SOS_STAGES = 2;

static I2SClass _i2s;

// ── Ics43434::begin ──────────────────────────────────────────
bool Ics43434::begin() {
    // Arduino ESP32 Core 3.x I2S API
    _i2s.setPins(PIN_I2S_SCK, PIN_I2S_WS, -1, PIN_I2S_SD);

    if (!_i2s.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_32BIT,
                    I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT)) {
        Serial.println("[ICS43434] ERROR: I2S begin() failed");
        Serial.println("[ICS43434] Check: SCK=GPIO10, WS=GPIO11, SD=GPIO12, L/R=GND");
        _ready = false;
        return false;
    }

    _ready = true;
    Serial.println("[ICS43434] Initialised — I2S 44100Hz 32-bit MONO LEFT");
    Serial.printf("[ICS43434] SCK=GPIO%d  WS=GPIO%d  SD=GPIO%d\n",
                  PIN_I2S_SCK, PIN_I2S_WS, PIN_I2S_SD);
    return true;
}

// ── Ics43434::read ───────────────────────────────────────────
bool Ics43434::read(SensorReadings& r) {
    if (!_ready) {
        r.noise_ok = false;
        return false;
    }

    // Static sample buffer — 44100 × 4 bytes = 176 KB
    // On ESP32-C6 with 320KB RAM this is tight; use heap
    static int32_t* samples = nullptr;
    if (!samples) {
        samples = (int32_t*)malloc(SAMPLES_COUNT * sizeof(int32_t));
        if (!samples) {
            Serial.println("[ICS43434] ERROR: malloc failed for sample buffer");
            _ready = false;
            return false;
        }
    }

    // Read 1-second window via new I2S API
    size_t bytesRead = _i2s.readBytes((char*)samples,
                                       SAMPLES_COUNT * sizeof(int32_t));

    if (bytesRead == 0) {
        Serial.println("[ICS43434] Read timeout");
        r.noise_ok = false;
        return false;
    }

    size_t sampleCount = bytesRead / sizeof(int32_t);

    float sum_sq = 0.0f;
    float peak   = 0.0f;
    // IIR filter state (persistent across calls for continuity)
    static float z[SOS_STAGES][2] = {};

    for (size_t i = 0; i < sampleCount; i++) {
        // ICS-43434: left-justified 24-bit in 32-bit frame
        float s = (float)(samples[i] >> 8) / (float)(1 << 23);

        // A-weighting biquad cascade
        for (uint8_t st = 0; st < SOS_STAGES; st++) {
            float w = s - A_WEIGHT_SOS[st][3] * z[st][0]
                        - A_WEIGHT_SOS[st][4] * z[st][1];
            float out = A_WEIGHT_SOS[st][0] * w
                      + A_WEIGHT_SOS[st][1] * z[st][0]
                      + A_WEIGHT_SOS[st][2] * z[st][1];
            z[st][1] = z[st][0];
            z[st][0] = w;
            s = out;
        }

        sum_sq += s * s;
        if (fabsf(s) > peak) peak = fabsf(s);
    }

    float rms = sum_sq / (float)sampleCount;
    if (rms < 1e-10f) rms = 1e-10f;

    // ICS-43434: sensitivity -26 dBFS at 94 dB SPL
    float leq_db  = 94.0f + 26.0f + 10.0f * log10f(rms) + sensitivityOffset;
    float peak_db = 94.0f + 26.0f + 20.0f * log10f(peak + 1e-10f) + sensitivityOffset;

    leq_db  = constrain(leq_db,  20.0f, 130.0f);
    peak_db = constrain(peak_db, 20.0f, 130.0f);

    r.noise_db = leq_db;
    r.noise_ok = true;
    _peakDb    = peak_db;

    Serial.printf("[ICS43434] Leq=%.1f dB(A)  Peak=%.1f dB  samples=%u\n",
                  leq_db, peak_db, sampleCount);
    return true;
}

bool  Ics43434::isReady() { return _ready; }
float Ics43434::peakDb()  { return _peakDb; }
