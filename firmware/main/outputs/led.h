#pragma once
// ============================================================
//  EnvCube — WS2812B RGB LED driver
//  Full implementation in Phase 1 Step 3.
// ============================================================

#include <Arduino.h>
#include <FastLED.h>
#include "../alerts/alert_engine.h"
#include "../config.h"

class Led {
public:
    static void begin();
    static void setAlert(AlertLevel level);
    static void setColor(CRGB color);
    static void pulse(CRGB color);
    static void loop();
};
