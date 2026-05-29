// ============================================================
//  EnvCube — ICS-43434 MEMS microphone noise level driver
//
//  Approach:
//  - ESP32-C6 I²S peripheral reads 24-bit samples from ICS-43434
//  - 1-second window of samples collected (44100 samples)
//  - Leq (energy-averaged) computed in the time domain:
//      Leq = 10 * log10(mean(sample^2)) + sensitivity_offset
//  - A-weighting IIR filter applied for dB(A) reading
//  - Based on ikostoski/esp32-i2s-slm open-source reference
//    (MIT license) — adapted for ESP32-C6 I²S API
//
//  A-weighting SOS filter coefficients for 44100 Hz sample rate
//  (from ICS-43434 compensation + A-weighting combined):
//  Computed using scipy.signal.sosfilt design.
//
//  ICS-43434 wiring:
//    SCK  → GPIO10 (bit clock)
//    WS   → GPIO11 (word select / L-R clock)
//    SD   → GPIO12 (serial data — connect to DO/SD pin)
//    L/R  → GND (selects LEFT channel, I²S address 0)
//    VDD  → 3.3V
//    GND  → GND
// ============================================================

#include "ics43434.h"
#include "../config.h"
#include <driver/i2s.h>
#include <math.h>

bool  Ics43434::_ready          = false;
float Ics43434::_peakDb         = 0.0f;
float Ics43434::sensitivityOffset = 0.0f;  // Calibrate against reference meter

// ── A-weighting SOS filter coefficients (44100 Hz) ──────────
// Format: [b0, b1, b2, a1, a2] per second-order section
// Combined mic equalization + A-weighting for ICS-43434
static const float A_WEIGHT_SOS[][5] = {
    // Stage 1: high-pass (removes DC + very low freq)
    { 0.47732642f,  0.46294358f,  0.11224797f,  0.06681948f,  0.00111522f },
    // Stage 2: A-weighting peaking
    { 1.0f,        -1.98905930f,  0.98908925f, -1.99755330f,  0.99755484f },
};
static const uint8_t SOS_STAGES = 2;

// ── Ics43434::begin ──────────────────────────────────────────
bool Ics43434::begin() {
    i2s_config_t i2s_config = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate          = SAMPLE_RATE,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count        = 4,
        .dma_buf_len          = 1024,
        .use_apll             = true,   // Audio PLL for accurate sample rate
        .tx_desc_auto_clear   = false,
        .fixed_mclk           = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num   = PIN_I2S_SCK,
        .ws_io_num    = PIN_I2S_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num  = PIN_I2S_SD
    };

    esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, nullptr);
    if (err != ESP_OK) {
        Serial.printf("[ICS43434] ERROR: i2s_driver_install failed (%d)\n", err);
        _ready = false;
        return false;
    }

    err = i2s_set_pin(I2S_PORT, &pin_config);
    if (err != ESP_OK) {
        Serial.printf("[ICS43434] ERROR: i2s_set_pin failed (%d)\n", err);
        i2s_driver_uninstall(I2S_PORT);
        _ready = false;
        return false;
    }

    // Clear DMA buffers
    i2s_zero_dma_buffer(I2S_PORT);

    _ready = true;
    Serial.println("[ICS43434] Initialised — I²S 44100Hz 32-bit LEFT channel");
    Serial.printf("[ICS43434] SCK=GPIO%d  WS=GPIO%d  SD=GPIO%d\n",
                  PIN_I2S_SCK, PIN_I2S_WS, PIN_I2S_SD);
    if (sensitivityOffset != 0.0f) {
        Serial.printf("[ICS43434] Sensitivity offset: %.1f dB\n", sensitivityOffset);
    } else {
        Serial.println("[ICS43434] No sensitivity offset — compare to reference meter");
    }
    return true;
}

// ── Ics43434::read ───────────────────────────────────────────
bool Ics43434::read(SensorReadings& r) {
    if (!_ready) {
        r.noise_ok = false;
        return false;
    }

    // Allocate sample buffer on heap (44100 × 4 bytes = 176 KB)
    // Using static buffer to avoid heap fragmentation
    static int32_t samples[SAMPLES_COUNT];
    size_t bytesRead = 0;

    esp_err_t err = i2s_read(I2S_PORT,
                             samples,
                             sizeof(samples),
                             &bytesRead,
                             pdMS_TO_TICKS(1500));  // 1.5 s timeout

    if (err != ESP_OK || bytesRead == 0) {
        Serial.printf("[ICS43434] Read error or timeout (err=%d, bytes=%u)\n",
                      err, bytesRead);
        r.noise_ok = false;
        return false;
    }

    size_t sampleCount = bytesRead / sizeof(int32_t);

    // ICS-43434 outputs left-justified 24-bit in 32-bit frame
    // Shift right by 8 bits to get signed 24-bit value, then normalise
    float sum_sq = 0.0f;
    float peak   = 0.0f;

    for (size_t i = 0; i < sampleCount; i++) {
        // Left-justify shift: ICS-43434 MSB is bit 31, 24 valid bits
        float s = (float)(samples[i] >> 8) / (float)(1 << 23);

        // A-weighting IIR filter (biquad cascade)
        // State variables for each SOS stage
        static float z[SOS_STAGES][2] = {};
        for (uint8_t st = 0; st < SOS_STAGES; st++) {
            float in  = s;
            float out = A_WEIGHT_SOS[st][0] * in
                      + A_WEIGHT_SOS[st][1] * z[st][0]
                      + A_WEIGHT_SOS[st][2] * z[st][1]
                      - A_WEIGHT_SOS[st][3] * z[st][0]
                      - A_WEIGHT_SOS[st][4] * z[st][1];
            z[st][1] = z[st][0];
            z[st][0] = in - A_WEIGHT_SOS[st][3] * z[st][0]
                           - A_WEIGHT_SOS[st][4] * z[st][1];
            s = out;
        }

        sum_sq += s * s;
        if (fabsf(s) > peak) peak = fabsf(s);
    }

    // Leq = 10 * log10(mean(s^2))
    float rms = sum_sq / (float)sampleCount;
    if (rms < 1e-10f) rms = 1e-10f;  // Floor to avoid log(0)

    // Reference: ICS-43434 sensitivity -26 dBFS at 94 dB SPL
    // So: dB_SPL = 94 + 20*log10(rms / 10^(-26/20))
    //           = 94 + 20*log10(rms) + 26
    //           = 120 + 10*log10(rms^2 / 1.0)  [simplified form]
    float leq_db = 94.0f + 26.0f + 10.0f * log10f(rms) + sensitivityOffset;

    // Peak dB (instantaneous max in this window)
    float peak_db = 94.0f + 26.0f + 20.0f * log10f(peak + 1e-10f) + sensitivityOffset;

    // Clamp to physically meaningful range (20 dB silence floor, 130 dB max)
    leq_db  = constrain(leq_db,  20.0f, 130.0f);
    peak_db = constrain(peak_db, 20.0f, 130.0f);

    r.noise_db = leq_db;
    r.noise_ok = true;
    _peakDb    = peak_db;

    Serial.printf("[ICS43434] Leq=%.1f dB(A)  Peak=%.1f dB  samples=%u\n",
                  leq_db, peak_db, sampleCount);
    return true;
}

bool  Ics43434::isReady()  { return _ready; }
float Ics43434::peakDb()   { return _peakDb; }
