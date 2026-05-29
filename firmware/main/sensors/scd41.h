#pragma once
// ============================================================
//  EnvCube — SCD41 CO₂ sensor driver
//  Sensirion SCD41 · I²C 0x62 · NDIR · ±40ppm
//  Smoke + CO₂ pod (Pod 2)
// ============================================================

#include <Arduino.h>
#include "../alerts/alert_engine.h"

class Scd41 {
public:
    static bool begin();
    static bool read(SensorReadings& r);
    static bool isReady();

    // Force re-calibration to known CO₂ concentration (ppm).
    // Call in a well-ventilated outdoor area with ~420ppm CO₂.
    // Only needed if readings drift significantly over time.
    static bool forceCalibration(uint16_t target_ppm = 420);

private:
    static bool     _ready;
    static uint32_t _lastMeasureMs;

    // SCD41 minimum measurement interval is 5 seconds
    static const uint32_t MEASURE_INTERVAL_MS = 5000;
};
