// ============================================================
//  EnvCube — ESP-NOW mesh stub
//  TODO: Full implementation in Phase 1 Step 5
// ============================================================

#include "espnow_mesh.h"

bool EspNowMesh::begin() {
    Serial.println("[ESP-NOW] Stub — not yet implemented (Step 5)");
    return true;
}

bool EspNowMesh::broadcast(const EspNowAlertPayload& payload) {
    Serial.printf("[ESP-NOW] Stub broadcast — level=%d source=%d room=%s\n",
                  (int)payload.level, (int)payload.source, payload.room_name);
    return true;
}

void EspNowMesh::loop() {}
