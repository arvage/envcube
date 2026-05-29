#pragma once
// ============================================================
//  EnvCube — Alert type definitions
//  Lightweight header — no dependencies.
//  Include this instead of alert_engine.h when you only need
//  AlertLevel / AlertSource (outputs, display, etc.)
// ============================================================

enum class AlertLevel {
    ALL_CLEAR  = 0,
    WARNING    = 1,
    ALERT      = 2,
    CRITICAL   = 3,
};

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
