#pragma once
// ============================================================
//  EnvCube — Alert types + sensor readings
//  Lightweight header — Arduino.h only, no project dependencies.
//  Include this anywhere you need AlertLevel, AlertSource,
//  or SensorReadings without pulling in the full alert engine.
// ============================================================

#include <Arduino.h>

// ── Alert severity ───────────────────────────────────────────
enum class AlertLevel {
    ALL_CLEAR  = 0,
    WARNING    = 1,
    ALERT      = 2,
    CRITICAL   = 3,
};

// ── Alert source ─────────────────────────────────────────────
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
    REMOTE      = 9,
};

// ── Live sensor data ─────────────────────────────────────────
// All sensor drivers write to this struct.
// Alert engine, display, MQTT all read from it.
struct SensorReadings {
    // Thermal pod (SHT40 + BMP280)
    float    temperature_c   = 0.0f;
    float    humidity_rh     = 0.0f;
    float    pressure_hpa    = 0.0f;
    bool     thermal_ok      = false;

    // Smoke + CO₂ pod
    uint16_t smoke_raw       = 0;
    uint16_t co2_ppm         = 0;
    bool     smoke_ok        = false;
    bool     co2_ok          = false;

    // Air quality pod
    int16_t  voc_index       = 0;
    int16_t  nox_index       = 0;
    uint32_t lux             = 0;
    bool     aq_ok           = false;

    // Particulate pod
    uint16_t pm1_0           = 0;
    uint16_t pm2_5           = 0;
    uint16_t pm10            = 0;
    bool     pm_ok           = false;

    // Presence pod
    bool     presence        = false;
    uint16_t presence_cm     = 0;
    bool     presence_ok     = false;

    // Noise pod
    float    noise_db        = 0.0f;
    bool     noise_ok        = false;

    unsigned long last_update_ms = 0;
};
