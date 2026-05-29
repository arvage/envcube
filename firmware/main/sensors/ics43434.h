#pragma once
// ============================================================
//  EnvCube — ICS-43434 MEMS microphone / noise level driver
//  TDK InvenSense ICS-43434 · I²S · Noise pod (Pod 6)
//
//  Measures: Leq (equivalent continuous sound level) in dB(A)
//  Factory calibrated: ±1 dB sensitivity — no field calibration
//  SNR: 61 dB(A) · Frequency range: 20 Hz – 20 kHz
//
//  Output: 1-second Leq window — not instantaneous peak.
//  A-weighting filter applied in firmware for dB(A) reading.
// ============================================================

#include <Arduino.h>
#include "../alerts/alert_engine.h"

class Ics43434 {
public:
    static bool begin();
    static bool read(SensorReadings& r);
    static bool isReady();

    // Returns instantaneous peak level from last window (dB SPL)
    static float peakDb();

    // Mic sensitivity correction offset (dB).
    // ICS-43434 sensitivity: -26 dBFS at 94 dB SPL (1 kHz).
    // Adjust if readings differ from a calibrated reference meter.
    static float sensitivityOffset;

private:
    static bool  _ready;
    static float _peakDb;

    // I²S sample processing
    static float  _computeLeq(int32_t* samples, size_t count);
    static float  _applyAWeighting(float* magnitudes, size_t count,
                                    float sampleRate);

    // I²S config
    static const uint32_t SAMPLE_RATE    = 44100;
    static const size_t   SAMPLES_COUNT  = SAMPLE_RATE;  // 1-second window
    static const i2s_port_t I2S_PORT     = I2S_NUM_0;
};
