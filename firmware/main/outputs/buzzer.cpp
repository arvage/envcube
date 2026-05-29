// EnvCube — Buzzer stub (full impl in Step 3)
#include "buzzer.h"
#include "../config.h"
void Buzzer::begin() { pinMode(PIN_BUZZER, OUTPUT); Serial.println("[Buzzer] Stub"); }
void Buzzer::alarm() { Serial.println("[Buzzer] ALARM (stub)"); }
void Buzzer::beep(uint8_t count) { Serial.printf("[Buzzer] BEEP x%u (stub)\n", count); }
void Buzzer::stop() {}
