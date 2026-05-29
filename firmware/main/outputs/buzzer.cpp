// ============================================================
//  EnvCube — Passive piezo buzzer driver (full implementation)
//
//  Uses ESP32 Arduino tone(pin, freq) / noTone(pin) API.
//  GPIO9 configured as output, driven by LEDC PWM internally.
//
//  loop() must be called regularly for timed patterns.
//  All timing is non-blocking — no delay() calls.
// ============================================================

#include "buzzer.h"
#include "../config.h"

bool     Buzzer::_muted          = false;
bool     Buzzer::_alarming       = false;
uint8_t  Buzzer::_beepCount      = 0;
uint8_t  Buzzer::_beepRemaining  = 0;
uint16_t Buzzer::_beepFreq       = BUZZ_FREQ_ALERT;
bool     Buzzer::_beepOn         = false;
uint32_t Buzzer::_lastToggleMs   = 0;
uint32_t Buzzer::_alarmLastMs    = 0;

// ── Buzzer::begin ────────────────────────────────────────────
void Buzzer::begin() {
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);
    Serial.println("[Buzzer] Initialised — GPIO" + String(PIN_BUZZER));
}

// ── Buzzer::alarm ────────────────────────────────────────────
void Buzzer::alarm() {
    if (!g_config.buzzer_enabled || _muted) return;
    _alarming      = true;
    _beepCount     = 0;  // clear any pending beep sequence
    _alarmLastMs   = millis();
    tone(PIN_BUZZER, BUZZ_FREQ_CRITICAL);
    Serial.println("[Buzzer] ALARM — continuous critical tone");
}

// ── Buzzer::beep ─────────────────────────────────────────────
void Buzzer::beep(uint8_t count, uint16_t freq) {
    if (!g_config.buzzer_enabled || _muted || _alarming) return;
    _beepCount     = count;
    _beepRemaining = count;
    _beepFreq      = freq;
    _beepOn        = true;
    _lastToggleMs  = millis();
    tone(PIN_BUZZER, freq);
    Serial.printf("[Buzzer] Beep x%u @ %u Hz\n", count, freq);
}

// ── Buzzer::confirm ──────────────────────────────────────────
void Buzzer::confirm() {
    if (!g_config.buzzer_enabled || _muted) return;
    tone(PIN_BUZZER, BUZZ_FREQ_OK, 200);  // 200 ms at 880 Hz
    Serial.println("[Buzzer] Confirm beep");
}

// ── Buzzer::stop ─────────────────────────────────────────────
void Buzzer::stop() {
    _alarming      = false;
    _beepCount     = 0;
    _beepRemaining = 0;
    _beepOn        = false;
    noTone(PIN_BUZZER);
    digitalWrite(PIN_BUZZER, LOW);
}

// ── Buzzer::mute / isMuted ───────────────────────────────────
void Buzzer::mute(bool muted) {
    _muted = muted;
    if (muted) stop();
    Serial.printf("[Buzzer] %s\n", muted ? "Muted" : "Unmuted");
}

bool Buzzer::isMuted() { return _muted; }

// ── Buzzer::loop ─────────────────────────────────────────────
void Buzzer::loop() {
    uint32_t now = millis();

    // ── Alarm mode: rapid on/off ──────────────────────────────
    if (_alarming) {
        if (_beepOn && now - _alarmLastMs >= ALARM_ON_MS) {
            noTone(PIN_BUZZER);
            _beepOn      = false;
            _alarmLastMs = now;
        } else if (!_beepOn && now - _alarmLastMs >= ALARM_OFF_MS) {
            tone(PIN_BUZZER, BUZZ_FREQ_CRITICAL);
            _beepOn      = true;
            _alarmLastMs = now;
        }
        return;
    }

    // ── Beep sequence ─────────────────────────────────────────
    if (_beepRemaining > 0) {
        if (_beepOn && now - _lastToggleMs >= BEEP_ON_MS) {
            noTone(PIN_BUZZER);
            _beepOn       = false;
            _lastToggleMs = now;
        } else if (!_beepOn && now - _lastToggleMs >= BEEP_OFF_MS) {
            if (--_beepRemaining > 0) {
                tone(PIN_BUZZER, _beepFreq);
                _beepOn       = true;
                _lastToggleMs = now;
            } else {
                // Sequence complete
                _beepCount = 0;
            }
        }
    }
}
