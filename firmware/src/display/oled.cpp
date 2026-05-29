// EnvCube — OLED display stub (full impl in Step 4)
#include "oled.h"
void OledDisplay::begin()                                         { Serial.println("[OLED] Stub"); }
void OledDisplay::update(const SensorReadings& r, AlertLevel l,
                          bool wifi, int rssi)                    { /* stub */ }
void OledDisplay::showBoot(const char* v)                         { Serial.printf("[OLED] Boot v%s\n", v); }
void OledDisplay::showProvisioning(const char* ssid)              { Serial.printf("[OLED] Provisioning: %s\n", ssid); }
void OledDisplay::nextScreen()                                    {}
