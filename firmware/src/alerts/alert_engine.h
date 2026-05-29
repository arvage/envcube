#pragma once
// ============================================================
//  EnvCube — Alert state machine
// ============================================================

#include <Arduino.h>
#include "../alert_types.h"

// ── Live sensor data struct ──────────────────────────────────
struct SensorReadings {
    float    temperature_c   = 0.0f;
    float    humidity_rh     = 0.0f;
    float    pressure_hpa    = 0.0f;
    bool     thermal_ok      = false;

    uint16_t smoke_raw       = 0;
    uint16_t co2_ppm         = 0;
    bool     smoke_ok        = false;
    bool     co2_ok          = false;

    int16_t  voc_index       = 0;
    int16_t  nox_index       = 0;
    uint32_t lux             = 0;
    bool     aq_ok           = false;

    uint16_t pm1_0           = 0;
    uint16_t pm2_5           = 0;
    uint16_t pm10            = 0;
    bool     pm_ok           = false;

    bool     presence        = false;
    uint16_t presence_cm     = 0;
    bool     presence_ok     = false;

    float    noise_db        = 0.0f;
    bool     noise_ok        = false;

    unsigned long last_update_ms = 0;
};

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

// ── Public API ───────────────────────────────────────────────
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

extern SensorReadings g_readings;
