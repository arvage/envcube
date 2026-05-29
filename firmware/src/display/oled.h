#pragma once
// EnvCube — OLED display stub (full impl in Step 4)
#include <Arduino.h>
#include "../alerts/alert_engine.h"
class OledDisplay {
public:
    static void begin();
    static void update(const SensorReadings& r, AlertLevel level,
                       bool wifiConnected, int rssi);
    static void showBoot(const char* version);
    static void showProvisioning(const char* ssid);
    static void nextScreen();
};
