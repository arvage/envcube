#pragma once
// ============================================================
//  EnvCube — BMP280 barometric pressure driver
//  Bosch BMP280 · I²C 0x77 · ±1 hPa · Thermal pod (Pod 1)
// ============================================================

#include <Arduino.h>
#include "../alerts/alert_engine.h"

class Bmp280Driver {
public:
    static bool begin();
    static bool read(SensorReadings& r);
    static bool isReady();

    // Sea-level pressure reference (hPa) for altitude calculation.
    // Standard atmosphere = 1013.25 hPa.
    static float seaLevelPressure;

private:
    static bool _ready;
};
