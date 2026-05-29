#pragma once
// ============================================================
//  EnvCube — Alert state machine
//  Evaluates all sensor readings against thresholds.
//  Drives: LED colour, buzzer, voice alert, MQTT publish.
//  Integrates ESP-NOW mesh for cross-cube alerting.
// ============================================================

#include <Arduino.h>

// ── Alert severity levels ────────────────────────────────────
enum class AlertLevel {
    ALL_CLEAR  = 0,   // 🟢 Everything normal
    WARNING    = 1,   // 🟡 One or more readings elevated
    ALERT      = 2,   // 🔴 Action needed
    CRITICAL   = 3,   // 🔴 + buzzer + voice (smoke, CO2 critical)
};

// ── Alert source (which sensor triggered) ────────────────────
enum class AlertSource {
    NONE        = 0,
    SMOKE       = 1,
    CO2         = 2,
    TEMPERATURE = 3,
    HUMIDITY    = 4,
    VOC         = 5,
    NOX         = 6,
    PM25        = 7,
    NOISE       = 8,
    REMOTE      = 9,  // received via ESP-NOW from another cube
};

// ── Live sensor data struct ──────────────────────────────────
// All sensor drivers write to this; alert engine reads it.
struct SensorReadings {
    // Thermal pod (SHT40 + BMP280)
    float    temperature_c   = 0.0f;
    float    humidity_rh     = 0.0f;
    float    pressure_hpa    = 0.0f;
    bool     thermal_ok      = false;

    // Smoke + CO₂ pod
    uint16_t smoke_raw       = 0;      // MQ-2 ADC 0–4095
    uint16_t co2_ppm         = 0;      // SCD41
    bool     smoke_ok        = false;
    bool     co2_ok          = false;

    // Air quality pod
    int16_t  voc_index       = 0;      // SGP41 0–500
    int16_t  nox_index       = 0;      // SGP41 0–500
    uint32_t lux             = 0;      // VEML7700
    bool     aq_ok           = false;

    // Particulate pod
    uint16_t pm1_0           = 0;      // µg/m³
    uint16_t pm2_5           = 0;
    uint16_t pm10            = 0;
    bool     pm_ok           = false;

    // Presence pod
    bool     presence        = false;  // LD2410C: someone in room?
    uint16_t presence_cm     = 0;      // distance in cm
    bool     presence_ok     = false;

    // Noise pod
    float    noise_db        = 0.0f;   // ICS-43434 Leq dB(A)
    bool     noise_ok        = false;

    // Timestamps
    unsigned long last_update_ms = 0;
};

// ── ESP-NOW alert payload (broadcast to peers) ───────────────
struct EspNowAlertPayload {
    uint8_t     version     = 1;
    AlertLevel  level;
    AlertSource source;
    char        room_name[32];
    uint8_t     cube_id;
    uint32_t    timestamp_s;
    float       value;          // reading that triggered alert
};

// ── Public API ───────────────────────────────────────────────
class AlertEngine {
public:
    // Initialise — call once from setup()
    static void begin();

    // Evaluate current readings against thresholds.
    // Called every POLL_FAST_MS from sensor task.
    static void evaluate(const SensorReadings& readings);

    // Handle an incoming ESP-NOW alert from another cube.
    // Called from ESP-NOW receive callback (IRAM-safe wrapper).
    static void onRemoteAlert(const EspNowAlertPayload& payload);

    // Current alert level.
    static AlertLevel currentLevel();

    // Current alert source (what triggered it).
    static AlertSource currentSource();

    // Human-readable description of current alert.
    static const char* currentMessage();

    // True if presence sensor says room is occupied.
    static bool roomOccupied();

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

// ── Global sensor readings — all modules share this ──────────
extern SensorReadings g_readings;
