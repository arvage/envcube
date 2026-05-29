#pragma once
// ============================================================
//  EnvCube — Passive piezo buzzer driver
//  PWM on GPIO9 · tone() / noTone() API
//
//  Passive buzzer = frequency-controllable via PWM.
//  Active buzzer = just toggles on/off (do NOT use active).
//
//  Alert patterns:
//  - WARNING   : single short beep every 30 s
//  - ALERT     : two beeps every 10 s
//  - CRITICAL  : continuous fast beep (smoke/CO2)
//  - ALL_CLEAR : silence
// ============================================================

#include <Arduino.h>
#include "../alert_types.h"

// Buzzer tone frequencies (Hz)
#define BUZZ_FREQ_WARN     1000
#define BUZZ_FREQ_ALERT    2000
#define BUZZ_FREQ_CRITICAL 3000
#define BUZZ_FREQ_OK        880

class Buzzer {
public:
    static void begin();

    // Continuous rapid alarm (smoke, CO2 critical)
    static void alarm();

    // N short beeps
    static void beep(uint8_t count, uint16_t freq = BUZZ_FREQ_ALERT);

    // Single confirmation beep (e.g. WiFi connected)
    static void confirm();

    // Stop all buzzing immediately
    static void stop();

    // Call from loop() — manages timed patterns
    static void loop();

    // Silence buzzer completely (user override)
    static void mute(bool muted);
    static bool isMuted();

private:
    static bool      _muted;
    static bool      _alarming;
    static uint8_t   _beepCount;
    static uint8_t   _beepRemaining;
    static uint16_t  _beepFreq;
    static bool      _beepOn;
    static uint32_t  _lastToggleMs;
    static uint32_t  _alarmLastMs;

    static const uint32_t ALARM_ON_MS   = 200;
    static const uint32_t ALARM_OFF_MS  = 100;
    static const uint32_t BEEP_ON_MS    = 150;
    static const uint32_t BEEP_OFF_MS   = 120;
    static const uint32_t BEEP_GAP_MS   = 300;  // gap between beep groups
};
