#pragma once
// ============================================================
//  EnvCube — VEML7700 ambient light driver
//  Vishay VEML7700 · I²C 0x10 · Air quality pod (Pod 3)
//  Measures: lux (0.0036–120,000 lux)
// ============================================================

#include <Arduino.h>
#include "../alerts/alert_engine.h"

class Veml7700Driver {
public:
    static bool begin();
    static bool read(SensorReadings& r);
    static bool isReady();

private:
    static bool _ready;
};
