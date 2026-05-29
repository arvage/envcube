#pragma once
// ============================================================
//  EnvCube — SGP41 VOC + NOx index driver
//  Sensirion SGP41 · I²C 0x59 · Air quality pod (Pod 3)
//
//  SGP41 outputs two indices (not raw resistance):
//  VOC Index: 0–500, lower = better (typical indoor: 100)
//  NOx Index: 1–500, lower = better (typical indoor: 1)
// ============================================================

#include <Arduino.h>
#include "../alert_types.h"

class Sgp41 {
public:
    static bool begin();

    // Requires SHT40 data for humidity compensation.
    // Pass temperature (°C) and humidity (%RH) from g_readings.
    static bool read(SensorReadings& r,
                     float temperature_c,
                     float humidity_rh);

    static bool isReady();

private:
    static bool     _ready;
    static bool     _conditioning;
    static uint32_t _conditioningStartMs;
    static uint32_t _lastReadMs;

    // SGP41 requires 10-second conditioning on first run
    static const uint32_t CONDITIONING_MS   = 10000;
    // Minimum 1-second interval between reads
    static const uint32_t READ_INTERVAL_MS  = 1000;
};
