#pragma once
// ============================================================
//  EnvCube — ESP-NOW encrypted mesh
//  Full implementation in Phase 1 Step 5.
//  This stub allows alert_engine.cpp to compile now.
// ============================================================

#include <Arduino.h>
#include "alert_engine.h"

class EspNowMesh {
public:
    static bool begin();
    static bool broadcast(const EspNowAlertPayload& payload);
    static void loop();
};
