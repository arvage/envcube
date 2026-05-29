// ============================================================
//  EnvCube — Alert state machine implementation
// ============================================================

#include "alert_engine.h"
#include "../storage/nvs_config.h"
#include "../config.h"
#include "../outputs/led.h"
#include "../outputs/buzzer.h"
#include "../outputs/dfplayer.h"
#include "espnow_mesh.h"
#ifdef ENVCUBE_ENABLE_MQTT
#include "../connectivity/mqtt_client.h"
#endif

SensorReadings g_readings = {};

AlertLevel  AlertEngine::_level   = AlertLevel::ALL_CLEAR;
AlertSource AlertEngine::_source  = AlertSource::NONE;
char        AlertEngine::_message[128] = "All clear";
bool        AlertEngine::_occupied = false;

// ── AlertEngine::begin ───────────────────────────────────────
void AlertEngine::begin() {
    _level  = AlertLevel::ALL_CLEAR;
    _source = AlertSource::NONE;
    snprintf(_message, sizeof(_message), "All clear");
    Serial.println("[Alert] Engine initialised");
}

// ── AlertEngine::evaluate ────────────────────────────────────
void AlertEngine::evaluate(const SensorReadings& readings) {
    // Update presence state
    if (readings.presence_ok) {
        _occupied = readings.presence;
    }

    // Walk all sensors, find highest severity
    AlertLevel highest = AlertLevel::ALL_CLEAR;
    AlertSource trigger = AlertSource::NONE;
    float       triggerValue = 0.0f;
    char        triggerMsg[128] = "All clear";

    // ── Smoke — highest priority, no suppression ─────────────
    if (readings.smoke_ok) {
        AlertLevel l = _evaluateSmoke(readings.smoke_raw);
        if (l > highest) {
            highest = l;
            trigger = AlertSource::SMOKE;
            triggerValue = readings.smoke_raw;
            snprintf(triggerMsg, sizeof(triggerMsg),
                     "Smoke detected in %s", g_config.room_name);
        }
    }

    // ── CO₂ ──────────────────────────────────────────────────
    if (readings.co2_ok) {
        AlertLevel l = _evaluateCO2(readings.co2_ppm);
        if (l > highest) {
            highest = l;
            trigger = AlertSource::CO2;
            triggerValue = readings.co2_ppm;
            snprintf(triggerMsg, sizeof(triggerMsg),
                     "CO2 high %u ppm in %s", readings.co2_ppm, g_config.room_name);
        }
    }

    // ── Presence suppression for non-critical alerts ──────────
    // If room is empty and suppress flag is set, cap at WARNING
    bool suppress = (g_config.presence_suppress_alerts && !_occupied
                     && readings.presence_ok);

    // ── Temperature ───────────────────────────────────────────
    if (readings.thermal_ok && !suppress) {
        AlertLevel l = _evaluateTemperature(readings.temperature_c);
        if (l > highest) {
            highest = l;
            trigger = AlertSource::TEMPERATURE;
            triggerValue = readings.temperature_c;
            snprintf(triggerMsg, sizeof(triggerMsg),
                     "Temperature %.1f C in %s",
                     readings.temperature_c, g_config.room_name);
        }
    }

    // ── VOC ───────────────────────────────────────────────────
    if (readings.aq_ok && !suppress) {
        AlertLevel l = _evaluateVOC(readings.voc_index);
        if (l > highest) {
            highest = l;
            trigger = AlertSource::VOC;
            triggerValue = readings.voc_index;
            snprintf(triggerMsg, sizeof(triggerMsg),
                     "Poor air quality VOC %d in %s",
                     readings.voc_index, g_config.room_name);
        }
    }

    // ── PM2.5 ─────────────────────────────────────────────────
    if (readings.pm_ok && !suppress) {
        AlertLevel l = _evaluatePM25(readings.pm2_5);
        if (l > highest) {
            highest = l;
            trigger = AlertSource::PM25;
            triggerValue = readings.pm2_5;
            snprintf(triggerMsg, sizeof(triggerMsg),
                     "Particulates %u ug/m3 in %s",
                     readings.pm2_5, g_config.room_name);
        }
    }

    // ── Noise ─────────────────────────────────────────────────
    if (readings.noise_ok && !suppress) {
        AlertLevel l = _evaluateNoise(readings.noise_db);
        if (l > highest) {
            highest = l;
            trigger = AlertSource::NOISE;
            triggerValue = readings.noise_db;
            snprintf(triggerMsg, sizeof(triggerMsg),
                     "High noise %.0f dB in %s",
                     readings.noise_db, g_config.room_name);
        }
    }

    // ── Apply final state ─────────────────────────────────────
    _applyLevel(highest, trigger, triggerMsg, triggerValue);
}

// ── AlertEngine::onRemoteAlert ───────────────────────────────
void AlertEngine::onRemoteAlert(const EspNowAlertPayload& payload) {
    Serial.printf("[Alert] Remote alert from %s: level=%d source=%d\n",
                  payload.room_name, (int)payload.level, (int)payload.source);

    // Always process CRITICAL remote alerts, even if room is empty
    if (payload.level >= AlertLevel::ALERT) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Alert from %s", payload.room_name);

        // Only upgrade our local level if remote is higher
        if (payload.level > _level) {
            _applyLevel(payload.level, AlertSource::REMOTE, msg, payload.value);
        } else {
            // Same or lower — still trigger outputs to alert occupants
            _triggerOutputs(payload.level, AlertSource::REMOTE);
        }
    }
}

// ── AlertEngine::currentLevel ────────────────────────────────
AlertLevel AlertEngine::currentLevel()   { return _level; }
AlertSource AlertEngine::currentSource() { return _source; }
const char* AlertEngine::currentMessage(){ return _message; }
bool AlertEngine::roomOccupied()         { return _occupied; }

// ── Private: threshold evaluators ────────────────────────────
AlertLevel AlertEngine::_evaluateSmoke(uint16_t raw) {
    if (raw >= g_config.thresh_smoke_alert) return AlertLevel::CRITICAL;
    if (raw >= g_config.thresh_smoke_warn)  return AlertLevel::WARNING;
    return AlertLevel::ALL_CLEAR;
}

AlertLevel AlertEngine::_evaluateCO2(uint16_t ppm) {
    if (ppm >= THRESH_CO2_CRITICAL)             return AlertLevel::CRITICAL;
    if (ppm >= g_config.thresh_co2_alert)       return AlertLevel::ALERT;
    if (ppm >= g_config.thresh_co2_warn)        return AlertLevel::WARNING;
    return AlertLevel::ALL_CLEAR;
}

AlertLevel AlertEngine::_evaluateTemperature(float c) {
    if (c >= g_config.thresh_temp_alert) return AlertLevel::ALERT;
    if (c >= g_config.thresh_temp_warn)  return AlertLevel::WARNING;
    return AlertLevel::ALL_CLEAR;
}

AlertLevel AlertEngine::_evaluateHumidity(float rh) {
    if (rh >= THRESH_HUM_ALERT)       return AlertLevel::ALERT;
    if (rh >= THRESH_HUM_HIGH_WARN)   return AlertLevel::WARNING;
    if (rh <= THRESH_HUM_LOW_WARN)    return AlertLevel::WARNING;
    return AlertLevel::ALL_CLEAR;
}

AlertLevel AlertEngine::_evaluateVOC(int16_t idx) {
    if (idx >= THRESH_VOC_ALERT)  return AlertLevel::ALERT;
    if (idx >= THRESH_VOC_WARN)   return AlertLevel::WARNING;
    return AlertLevel::ALL_CLEAR;
}

AlertLevel AlertEngine::_evaluatePM25(uint16_t ugm3) {
    if (ugm3 >= THRESH_PM25_CRITICAL) return AlertLevel::CRITICAL;
    if (ugm3 >= g_config.thresh_pm25_alert) return AlertLevel::ALERT;
    if (ugm3 >= g_config.thresh_pm25_warn)  return AlertLevel::WARNING;
    return AlertLevel::ALL_CLEAR;
}

AlertLevel AlertEngine::_evaluateNoise(float db) {
    if (db >= THRESH_NOISE_CRITICAL)          return AlertLevel::CRITICAL;
    if (db >= g_config.thresh_noise_alert)    return AlertLevel::ALERT;
    if (db >= g_config.thresh_noise_warn)     return AlertLevel::WARNING;
    return AlertLevel::ALL_CLEAR;
}

// ── _applyLevel ──────────────────────────────────────────────
void AlertEngine::_applyLevel(AlertLevel level, AlertSource source,
                               const char* message, float value) {
    bool levelChanged = (level != _level) || (source != _source);

    _level  = level;
    _source = source;
    strncpy(_message, message, sizeof(_message) - 1);

    if (levelChanged) {
        Serial.printf("[Alert] Level changed: %d (%s) val=%.1f\n",
                      (int)level, message, value);
#ifdef ENVCUBE_ENABLE_MQTT
        // Publish immediately on state change (don't wait for poll interval)
        MqttClient::publishAlert(level, source, message);
#endif
    }

    _triggerOutputs(level, source);

    // Broadcast CRITICAL + ALERT via ESP-NOW mesh
    if (level >= AlertLevel::ALERT && source != AlertSource::REMOTE) {
        _broadcastAlert(level, source, message, value);
    }
}

// ── _triggerOutputs ──────────────────────────────────────────
void AlertEngine::_triggerOutputs(AlertLevel level, AlertSource source) {
    // LED
    Led::setAlert(level);

    // Buzzer + voice — only for CRITICAL and when room is occupied
    // (REMOTE alerts override presence suppression)
    bool forceAlert = (source == AlertSource::REMOTE
                       || source == AlertSource::SMOKE
                       || source == AlertSource::CO2);

    if (level == AlertLevel::CRITICAL && (forceAlert || _occupied)) {
        if (g_config.buzzer_enabled) Buzzer::alarm();
        if (g_config.voice_enabled)  DFPlayer::playForSource(source);
    } else if (level == AlertLevel::ALERT) {
        if (g_config.buzzer_enabled) Buzzer::beep(2);
    } else if (level == AlertLevel::ALL_CLEAR) {
        Buzzer::stop();
    }
}

// ── _broadcastAlert ──────────────────────────────────────────
void AlertEngine::_broadcastAlert(AlertLevel level, AlertSource source,
                                   const char* message, float value) {
#ifdef ENVCUBE_ENABLE_ESPNOW
    EspNowMesh::broadcastAlert(level, source, g_config.room_name, value);
#endif
}
