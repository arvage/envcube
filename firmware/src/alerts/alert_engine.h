#pragma once
// ============================================================
//  EnvCube — Alert state machine
// ============================================================

#include <Arduino.h>
#include "../alert_types.h"

// ── ESP-NOW alert payload ────────────────────────────────────
struct EspNowAlertPayload {
    uint8_t     version     = 1;
    AlertLevel  level;
    AlertSource source;
    char        room_name[32];
    uint8_t     cube_id;
    uint32_t    timestamp_s;
    float       value;
};

// ── Alert engine ─────────────────────────────────────────────
class AlertEngine {
public:
    static void begin();
    static void evaluate(const SensorReadings& readings);
    static void onRemoteAlert(const EspNowAlertPayload& payload);
    static AlertLevel   currentLevel();
    static AlertSource  currentSource();
    static const char*  currentMessage();
    static bool         roomOccupied();

private:
    static AlertLevel   _level;
    static AlertSource  _source;
    static char         _message[128];
    static bool         _occupied;

    static AlertLevel _evaluateSmoke(uint16_t raw);
    static AlertLevel _evaluateCO2(uint16_t ppm);
    static AlertLevel _evaluateTemperature(float c);
    static AlertLevel _evaluateHumidity(float rh);
    static AlertLevel _evaluateVOC(int16_t idx);
    static AlertLevel _evaluatePM25(uint16_t ugm3);
    static AlertLevel _evaluateNoise(float db);

    static void _applyLevel(AlertLevel level, AlertSource source,
                            const char* message, float value);
    static void _broadcastAlert(AlertLevel level, AlertSource source,
                                const char* message, float value);
    static void _triggerOutputs(AlertLevel level, AlertSource source);
};

// Global sensor readings instance — all modules share this
extern SensorReadings g_readings;
