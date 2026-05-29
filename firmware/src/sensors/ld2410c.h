#pragma once
// ============================================================
//  EnvCube — HLK-LD2410C 24GHz mmWave presence driver
//  Hi-Link LD2410C · UART 256000 baud · Presence pod (Pod 4)
//
//  Detects:
//  - Moving targets (motion detection)
//  - Stationary targets (micro-movement: breathing, heartbeat)
//  - Distance to nearest target (cm)
//
//  Unlike PIR, LD2410C detects people sitting perfectly still.
// ============================================================

#include <Arduino.h>
#include "../alert_types.h"

class Ld2410c {
public:
    static bool begin();
    static bool read(SensorReadings& r);
    static bool isReady();

    // Returns true if presence was detected in last read
    static bool presenceDetected();

    // Distance to nearest detected target in cm (0 = no target)
    static uint16_t targetDistanceCm();

private:
    static bool     _ready;
    static bool     _presence;
    static uint16_t _distanceCm;

    // Parse LD2410C data frame from UART
    static bool _parseFrame(const uint8_t* buf, uint8_t len);

    // Frame constants
    static const uint8_t FRAME_HEADER_1 = 0xF4;
    static const uint8_t FRAME_HEADER_2 = 0xF3;
    static const uint8_t FRAME_HEADER_3 = 0xF2;
    static const uint8_t FRAME_HEADER_4 = 0xF1;
    static const uint8_t FRAME_END_1    = 0xF8;
    static const uint8_t FRAME_END_2    = 0xF7;
    static const uint8_t FRAME_END_3    = 0xF6;
    static const uint8_t FRAME_END_4    = 0xF5;
};
