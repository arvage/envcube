// EnvCube — LED stub (full impl in Step 3)
#include "led.h"

static CRGB _leds[1];

void Led::begin() {
    FastLED.addLeds<WS2812B, PIN_LED_DATA, GRB>(_leds, 1);
    FastLED.setBrightness(LED_BRIGHTNESS);
    _leds[0] = CRGB::Black;
    FastLED.show();
    Serial.println("[LED] Stub initialised");
}

void Led::setAlert(AlertLevel level) {
    switch (level) {
        case AlertLevel::ALL_CLEAR: setColor(LED_COLOR_GREEN);  break;
        case AlertLevel::WARNING:   setColor(LED_COLOR_AMBER);  break;
        case AlertLevel::ALERT:
        case AlertLevel::CRITICAL:  setColor(LED_COLOR_RED);    break;
    }
}

void Led::setColor(CRGB color) {
    _leds[0] = color;
    FastLED.show();
}

void Led::pulse(CRGB color) { setColor(color); }
void Led::loop() {}
