#pragma once
// ============================================================
//  EnvCube — WS2812B RGB LED driver
//  FastLED · single addressable LED · GPIO8
//
//  Modes:
//  - Solid colour (alert state)
//  - Pulse / breathe (provisioning, connecting)
//  - Flash (OTA, identify)
//  - Off (presence suppression at night)
// ============================================================

#include <Arduino.h>
#include <FastLED.h>
#include "../alerts/alert_engine.h"
#include "../config.h"

enum class LedMode {
    SOLID,    // Steady colour
    PULSE,    // Slow breathe in/out
    FLASH,    // Fast on/off
    OFF,
};

class Led {
public:
    // Call once from setup() — initialises FastLED
    static void begin();

    // Set colour and mode for current alert level.
    // Green=ALL_CLEAR  Amber=WARNING  Red=ALERT/CRITICAL
    static void setAlert(AlertLevel level);

    // Set a specific solid colour directly
    static void setColor(CRGB color);

    // Start a pulsing animation on a given colour
    static void pulse(CRGB color);

    // Flash N times then return to previous state
    static void flash(CRGB color, uint8_t times = 3);

    // Turn LED off (e.g. dark room + no alerts)
    static void off();

    // Call from loop() or a low-priority task to drive animations
    static void loop();

    // Dim or restore brightness (e.g. night mode)
    static void setBrightness(uint8_t b);

private:
    static CRGB      _leds[1];
    static LedMode   _mode;
    static CRGB      _targetColor;
    static CRGB      _prevColor;
    static uint8_t   _brightness;
    static uint32_t  _lastUpdateMs;

    // Pulse animation state
    static float     _pulsePhase;     // 0.0–2π
    static const float PULSE_SPEED;   // radians per ms

    // Flash animation state
    static uint8_t   _flashCount;
    static uint8_t   _flashRemaining;
    static bool      _flashOn;
    static uint32_t  _flashLastMs;
    static const uint32_t FLASH_INTERVAL_MS = 120;

    static void _applyPulse();
    static void _applyFlash();
};
