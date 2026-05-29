#pragma once
// ============================================================
//  EnvCube — SHT40 temperature + humidity driver
//  Sensirion SHT40-AD1B · I²C 0x44 · ±0.2°C ±1.8%RH
//  Thermal pod (Pod 1)
// ============================================================

#include <Arduino.h>
#include "../alerts/alert_engine.h"

class Sht40 {
public:
    // Initialise sensor. Returns false if not found on I²C bus.
    static bool begin();

    // Read latest values into g_readings.
    // Sets g_readings.thermal_ok = false on error.
    static bool read(SensorReadings& r);

    // True if sensor was found and initialised successfully.
    static bool isReady();

private:
    static bool _ready;
};
