#pragma once
// ============================================================
//  EnvCube — OLED display driver
//  SSD1306 128×64 I²C · Adafruit SSD1306 library
//
//  3 screens cycled by button tap:
//    0 — Indoor sensors (default)
//    1 — Alert status + presence
//    2 — System info + WiFi
//
//  Special screens:
//    Boot splash, provisioning, OTA progress
// ============================================================

#include <Arduino.h>
#include "../alert_types.h"

// Weather data populated by weather.cpp (Step 7)
struct WeatherData {
    float   temp_f       = 0.0f;
    float   humidity     = 0.0f;
    uint8_t weather_code = 0;     // WMO weather code
    float   uv_index     = 0.0f;
    bool    valid        = false;
    unsigned long fetched_ms = 0;
};

extern WeatherData g_weather;

class OledDisplay {
public:
    // Call once from setup()
    static void begin();

    // Call from display task every 2 s
    static void update(const SensorReadings& r,
                       AlertLevel level,
                       bool wifiConnected,
                       int rssi);

    // Special screens
    static void showBoot(const char* version);
    static void showProvisioning(const char* ssid);
    static void showOtaProgress(uint8_t percent);

    // Cycle to next screen (called on button tap)
    static void nextScreen();

    // Force a specific screen
    static void setScreen(uint8_t screen);

    // Dim/restore (night mode)
    static void setDim(bool dim);

    static uint8_t currentScreen();

private:
    static void _drawScreen0(const SensorReadings& r, AlertLevel level);
    static void _drawScreen1(const SensorReadings& r, AlertLevel level);
    static void _drawScreen2(bool wifiConnected, int rssi);

    static void _drawHeader(const char* title, AlertLevel level);
    static void _drawFooter(AlertLevel level, bool wifi);
    static void _drawAlertIcon(AlertLevel level, uint8_t x, uint8_t y);
    static const char* _alertLabel(AlertLevel level);
    static const char* _weatherDesc(uint8_t code);

    static uint8_t  _screen;
    static bool     _dimmed;
    static bool     _otaActive;
    static uint32_t _lastActivityMs;
    static bool     _ready;

    static const uint8_t SCREEN_COUNT    = 3;
    static const uint32_t DIM_TIMEOUT_MS = 30000;
};
