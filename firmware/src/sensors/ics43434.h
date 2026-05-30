#pragma once
// ============================================================
//  EnvCube — ICS-43434 MEMS microphone / noise level driver
//  TDK InvenSense ICS-43434 · I²S · Noise pod (Pod 6)
//
//  Measures: Leq (equivalent continuous sound level) in dB(A)
//  Factory calibrated: ±1 dB sensitivity — no field calibration
//  SNR: 61 dB(A) · Frequency range: 20 Hz – 20 kHz
//
//  Arduino ESP32 Core 3.x uses the new I2S API (ESP_I2S).
//  The old driver/i2s.h API is no longer available.
// ============================================================

#include <Arduino.h>
#include "../alert_types.h"

class Ics43434 {
public:
    static bool begin();
    static bool read(SensorReadings& r);
    static bool isReady();
    static float peakDb();

    // Mic sensitivity correction offset (dB).
    // Adjust if readings differ from a calibrated reference meter.
    static float sensitivityOffset;

private:
    static bool  _ready;
    static float _peakDb;

    static const uint32_t SAMPLE_RATE   = 44100;
    static const size_t   SAMPLES_COUNT = 4410;   // 100ms window — 17.6KB heap vs 176KB
};
