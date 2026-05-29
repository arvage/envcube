// ============================================================
//  EnvCube — Outputs task implementation
//  Drives LED animations, buzzer patterns, DFPlayer health
//  at 50Hz (20ms tick). Pinned to core 1 (same as display).
// ============================================================

#include "outputs_task.h"
#include "led.h"
#include "buzzer.h"
#include "dfplayer.h"

void startOutputsTask() {
    xTaskCreatePinnedToCore(
        outputsTaskFn, "outputs",
        2048, nullptr, 2, nullptr, 1
    );
    Serial.println("[Outputs] Task started");
}

void outputsTaskFn(void* param) {
    for (;;) {
        Led::loop();
        Buzzer::loop();
        DFPlayer::loop();
        vTaskDelay(pdMS_TO_TICKS(20));  // 50 Hz
    }
}
