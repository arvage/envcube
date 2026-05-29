#pragma once
// ============================================================
//  EnvCube — PMSA003I particulate matter driver
//  Plantower PMSA003I · I²C 0x12 · Particulate pod (Pod 5)
//  Measures: PM1.0, PM2.5, PM10 in µg/m³
//  Also reports particle counts per 0.1L air
// ============================================================

#include <Arduino.h>
#include "../alert_types.h"

class Pmsa003i {
public:
    static bool begin();
    static bool read(SensorReadings& r);
    static bool isReady();

private:
    static bool _ready;

    // PMSA003I produces a new frame every ~800ms
    static const uint32_t READ_INTERVAL_MS = 1000;
    static uint32_t _lastReadMs;
};
