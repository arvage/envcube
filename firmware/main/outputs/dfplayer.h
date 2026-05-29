#pragma once
// EnvCube — DFPlayer stub (full impl in Step 3)
#include <Arduino.h>
#include "../alerts/alert_engine.h"
class DFPlayer {
public:
    static void begin();
    static void play(uint8_t track);
    static void playForSource(AlertSource source);
};
