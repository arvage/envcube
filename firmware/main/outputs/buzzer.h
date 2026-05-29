#pragma once
// EnvCube — Buzzer stub (full impl in Step 3)
#include <Arduino.h>
class Buzzer {
public:
    static void begin();
    static void alarm();
    static void beep(uint8_t count);
    static void stop();
};
