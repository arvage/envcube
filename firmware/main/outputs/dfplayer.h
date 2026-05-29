#pragma once
// ============================================================
//  EnvCube — DFPlayer Mini voice alert driver
//  UART1 GPIO4/5 · 9600 baud · DFRobotDFPlayerMini library
//
//  Audio files on microSD (FAT32, numbered /01.mp3..):
//  01 = "Smoke detected in [room]"
//  02 = "CO2 level high"
//  03 = "CO2 level critical — ventilate now"
//  04 = "Temperature too high"
//  05 = "Humidity warning"
//  06 = "Poor air quality detected"
//  07 = "High particulate levels"
//  08 = "High noise level"
//  09 = "All clear"
//  10 = "WiFi connected"
//  11 = "Setup mode — connect to EnvCube Setup"
//
//  Room name spoken: firmware appends room by playing a
//  room-specific file named /room_<name>.mp3 if present,
//  otherwise the alert track suffices.
// ============================================================

#include <Arduino.h>
#include <DFRobotDFPlayerMini.h>
#include "../alerts/alert_engine.h"
#include "../config.h"

class DFPlayer {
public:
    // Initialise DFPlayer. Returns false if not detected.
    static bool begin();

    // Play a track by number (1-based, maps to /01.mp3 etc.)
    static void play(uint8_t track);

    // Play the appropriate alert voice for a given source
    static void playForSource(AlertSource source);

    // Stop current playback
    static void stop();

    // Set volume (0–30, default AUDIO_VOLUME from config)
    static void setVolume(uint8_t vol);

    // True if DFPlayer module was found and initialised
    static bool isReady();

    // Call from loop() — checks for module errors
    static void loop();

private:
    static DFRobotDFPlayerMini _player;
    static HardwareSerial      _serial;
    static bool                _ready;
    static uint8_t             _volume;
    static uint32_t            _lastPlayMs;

    // Minimum gap between consecutive plays (avoid overlap)
    static const uint32_t MIN_PLAY_GAP_MS = 3000;
};
