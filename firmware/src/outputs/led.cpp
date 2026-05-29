// ============================================================
//  EnvCube — WS2812B RGB LED driver (full implementation)
//
//  FastLED drives a single WS2812B on GPIO8.
//  loop() must be called regularly (every ~20 ms) to animate.
//
//  Alert colour mapping:
//    ALL_CLEAR → green  (solid)
//    WARNING   → amber  (solid)
//    ALERT     → red    (solid)
//    CRITICAL  → red    (fast flash — draws attention)
//
//  Special states:
//    Blue pulse  = WiFi provisioning / connecting
//    White pulse = OTA update in progress
//    White solid = booting
// ============================================================

#include "led.h"

CRGB     Led::_leds[1]          = {};
LedMode  Led::_mode             = LedMode::SOLID;
CRGB     Led::_targetColor      = CRGB::Black;
CRGB     Led::_prevColor        = CRGB::Black;
uint8_t  Led::_brightness       = LED_BRIGHTNESS;
uint32_t Led::_lastUpdateMs     = 0;
float    Led::_pulsePhase       = 0.0f;
const float Led::PULSE_SPEED    = 0.003f;  // ~2 s per cycle at 20ms tick
uint8_t  Led::_flashCount       = 0;
uint8_t  Led::_flashRemaining   = 0;
bool     Led::_flashOn          = false;
uint32_t Led::_flashLastMs      = 0;

// ── Led::begin ───────────────────────────────────────────────
void Led::begin() {
    FastLED.addLeds<WS2812B, PIN_LED_DATA, GRB>(_leds, 1);
    FastLED.setBrightness(_brightness);
    _leds[0] = CRGB::Black;
    FastLED.show();
    Serial.println("[LED] Initialised — WS2812B GPIO" + String(PIN_LED_DATA));
}

// ── Led::setAlert ────────────────────────────────────────────
void Led::setAlert(AlertLevel level) {
    switch (level) {
        case AlertLevel::ALL_CLEAR:
            setColor(LED_COLOR_GREEN);
            break;
        case AlertLevel::WARNING:
            setColor(LED_COLOR_AMBER);
            break;
        case AlertLevel::ALERT:
            setColor(LED_COLOR_RED);
            break;
        case AlertLevel::CRITICAL:
            // Fast flash red for critical — smoke/CO2
            flash(LED_COLOR_RED, 0);  // 0 = infinite flash
            break;
    }
}

// ── Led::setColor ────────────────────────────────────────────
void Led::setColor(CRGB color) {
    _mode        = LedMode::SOLID;
    _targetColor = color;
    _leds[0]     = color;
    FastLED.setBrightness(_brightness);
    FastLED.show();
}

// ── Led::pulse ───────────────────────────────────────────────
void Led::pulse(CRGB color) {
    _mode        = LedMode::PULSE;
    _targetColor = color;
    _pulsePhase  = 0.0f;
}

// ── Led::flash ───────────────────────────────────────────────
void Led::flash(CRGB color, uint8_t times) {
    _mode           = LedMode::FLASH;
    _targetColor    = color;
    _flashCount     = times;
    _flashRemaining = times;
    _flashOn        = true;
    _flashLastMs    = millis();
    _leds[0]        = color;
    FastLED.setBrightness(_brightness);
    FastLED.show();
}

// ── Led::off ─────────────────────────────────────────────────
void Led::off() {
    _mode    = LedMode::OFF;
    _leds[0] = CRGB::Black;
    FastLED.show();
}

// ── Led::setBrightness ───────────────────────────────────────
void Led::setBrightness(uint8_t b) {
    _brightness = b;
    FastLED.setBrightness(b);
    FastLED.show();
}

// ── Led::loop ────────────────────────────────────────────────
void Led::loop() {
    uint32_t now = millis();
    if (now - _lastUpdateMs < 20) return;  // 50 Hz max update rate
    _lastUpdateMs = now;

    switch (_mode) {
        case LedMode::PULSE: _applyPulse(); break;
        case LedMode::FLASH: _applyFlash(); break;
        default: break;
    }
}

// ── _applyPulse ──────────────────────────────────────────────
void Led::_applyPulse() {
    // Sine wave 0→1→0, maps to brightness 10→255→10
    _pulsePhase += PULSE_SPEED * 20.0f;  // 20 ms per tick
    if (_pulsePhase > TWO_PI) _pulsePhase -= TWO_PI;

    float s   = (sinf(_pulsePhase) + 1.0f) * 0.5f;  // 0.0–1.0
    uint8_t b = (uint8_t)(10 + s * 245.0f);          // 10–255

    _leds[0] = _targetColor;
    FastLED.setBrightness(b);
    FastLED.show();
}

// ── _applyFlash ──────────────────────────────────────────────
void Led::_applyFlash() {
    uint32_t now = millis();
    if (now - _flashLastMs < FLASH_INTERVAL_MS) return;
    _flashLastMs = now;

    _flashOn = !_flashOn;
    _leds[0] = _flashOn ? _targetColor : CRGB::Black;
    FastLED.setBrightness(_brightness);
    FastLED.show();

    // Finite flash count — return to previous state when done
    if (_flashCount > 0 && !_flashOn) {
        if (--_flashRemaining == 0) {
            setColor(_prevColor);
        }
    }
}
