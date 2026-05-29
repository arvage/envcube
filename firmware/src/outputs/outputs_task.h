#pragma once
// ============================================================
//  EnvCube — Outputs task
//  Runs LED animations, buzzer patterns, DFPlayer monitor
//  at ~50Hz in a dedicated low-priority FreeRTOS task.
//  Call startOutputsTask() once from setup().
// ============================================================
#include <Arduino.h>

void startOutputsTask();
void outputsTaskFn(void* param);
