#pragma once
// ============================================================
//  EnvCube — MQ-2 smoke sensor driver
//  Analog output · GPIO0 (ADC1 CH0) · Smoke + CO₂ pod (Pod 2)
//
//  MQ-2 detects: smoke, LPG, propane, hydrogen, CO
//  Output: analog voltage (0–3.3V) → 12-bit ADC (0–4095)
//
//  IMPORTANT: 60-second warm-up required on every power-on.
//  Firmware enforces this — readings suppressed during warm-up.
// ============================================================

#include <Arduino.h>
#include "../alert_types.h"

class Mq2 {
public:
    static bool begin();
    static bool read(SensorReadings& r);
    static bool isReady();
    static bool isWarmedUp();

    // Call during calibration: returns average raw ADC over N samples.
    // Run in clean air to establish baseline_clean_air value.
    static uint16_t calibrateCleanAir(uint8_t samples = 20);

    // Baseline raw ADC in clean air (default: conservative estimate).
    // Calibrate this for your specific MQ-2 module.
    static uint16_t baseline_clean_air;

private:
    static bool     _ready;
    static bool     _warmedUp;
    static uint32_t _bootTimeMs;

    // Number of ADC samples to average per reading (noise reduction)
    static const uint8_t  ADC_SAMPLES     = 10;
    // ADC attenuation for 0–3.3V range on ESP32-C6
    static const adc_attenuation_t ADC_ATTEN = ADC_11db;
};
