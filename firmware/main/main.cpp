// ============================================================
//  EnvCube — main.cpp
//  Boot sequence + FreeRTOS task launch
//  Phase 1 Step 3: full outputs wired
// ============================================================

#include <Arduino.h>
#include "config.h"
#include "storage/nvs_config.h"
#include "connectivity/wifi_manager.h"
#include "alerts/alert_engine.h"
#include "alerts/espnow_mesh.h"
#include "outputs/led.h"
#include "outputs/buzzer.h"
#include "outputs/dfplayer.h"
#include "outputs/outputs_task.h"
#include "display/oled.h"
// Sensor drivers
#include "sensors/sht40.h"
#include "sensors/bmp280.h"
#include "sensors/scd41.h"
#include "sensors/sgp41.h"
#include "sensors/veml7700.h"
#include "sensors/mq2.h"
#include "sensors/pmsa003i.h"
#include "sensors/ld2410c.h"
#include "sensors/ics43434.h"

// ── Forward declarations ──────────────────────────────────────
void taskSensors(void* param);
void taskDisplay(void* param);
void taskConnectivity(void* param);
void handleButton();

static unsigned long _btnPressStart = 0;
static bool          _btnWasPressed = false;

// ── setup ─────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("\n+=========================+");
    Serial.println("|  EnvCube v" ENVCUBE_VERSION "           |");
    Serial.println("|  Phase 1 prototype      |");
    Serial.println("+=========================+");

    pinMode(PIN_BUTTON, INPUT_PULLUP);

    // ── Outputs first — give user immediate visual feedback ───
    Led::begin();
    Led::setColor(LED_COLOR_WHITE);   // White = booting
    Buzzer::begin();
    DFPlayer::begin();                // ~1.5 s startup
    startOutputsTask();               // LED anim + buzzer + DFPlayer monitor

    // ── Config + alert engine ─────────────────────────────────
    NvsConfig::load();
    NvsConfig::dump();
    AlertEngine::begin();

    // ── Display ───────────────────────────────────────────────
    OledDisplay::begin();
    OledDisplay::showBoot(ENVCUBE_VERSION);

    // ── WiFi ──────────────────────────────────────────────────
    if (!g_config.is_provisioned) {
        Led::pulse(LED_COLOR_BLUE);
        OledDisplay::showProvisioning(WIFI_AP_SSID);
        if (g_config.voice_enabled) DFPlayer::play(AUDIO_PROVISIONING);
    } else {
        Led::pulse(LED_COLOR_WHITE);
    }
    WifiManager::begin();

    if (WifiManager::isConnected()) {
        Led::setColor(LED_COLOR_GREEN);
        if (g_config.voice_enabled) DFPlayer::play(AUDIO_WIFI_CONNECTED);
        Buzzer::confirm();
    } else {
        Led::setColor(LED_COLOR_AMBER);
    }

    EspNowMesh::begin();

    // ── Sensor + alert tasks ──────────────────────────────────
    xTaskCreatePinnedToCore(taskSensors,      "sensors",
                            8192, nullptr, 3, nullptr, 0);
    xTaskCreatePinnedToCore(taskDisplay,      "display",
                            4096, nullptr, 2, nullptr, 1);
    xTaskCreatePinnedToCore(taskConnectivity, "connectivity",
                            8192, nullptr, 1, nullptr, 1);

    Serial.println("[Boot] EnvCube running");
    Led::setAlert(AlertLevel::ALL_CLEAR);
}

// ── loop ──────────────────────────────────────────────────────
void loop() {
    WifiManager::loop();
    handleButton();
    delay(50);
}

// ── taskSensors ───────────────────────────────────────────────
void taskSensors(void* param) {
    Sht40::begin();
    Bmp280Driver::begin();
    Scd41::begin();
    Sgp41::begin();
    Veml7700Driver::begin();
    Mq2::begin();
    Pmsa003i::begin();
    Ld2410c::begin();
    Ics43434::begin();

    Serial.println("[Sensors] All initialised — polling started");
    unsigned long lastSlowPoll = 0;

    for (;;) {
        unsigned long now = millis();

        Sht40::read(g_readings);
        Bmp280Driver::read(g_readings);
        Mq2::read(g_readings);
        Ld2410c::read(g_readings);
        Pmsa003i::read(g_readings);
        Ics43434::read(g_readings);
        Veml7700Driver::read(g_readings);

        if (now - lastSlowPoll >= POLL_SLOW_MS) {
            Scd41::read(g_readings);
            Sgp41::read(g_readings,
                        g_readings.temperature_c,
                        g_readings.humidity_rh);
            lastSlowPoll = now;
        }

        g_readings.last_update_ms = millis();
        AlertEngine::evaluate(g_readings);

        vTaskDelay(pdMS_TO_TICKS(POLL_FAST_MS));
    }
}

// ── taskDisplay ───────────────────────────────────────────────
void taskDisplay(void* param) {
    for (;;) {
        OledDisplay::update(g_readings,
                            AlertEngine::currentLevel(),
                            WifiManager::isConnected(),
                            WifiManager::rssi());
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

// ── taskConnectivity ──────────────────────────────────────────
void taskConnectivity(void* param) {
    unsigned long lastHeartbeat = 0;
    for (;;) {
        if (!WifiManager::isConnected()) {
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }
        unsigned long now = millis();
        if (now - lastHeartbeat >= HEARTBEAT_INTERVAL_MS) {
            Serial.println("[Heartbeat] Ping");
            lastHeartbeat = now;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// ── handleButton ──────────────────────────────────────────────
void handleButton() {
    bool pressed = (digitalRead(PIN_BUTTON) == LOW);
    if (pressed && !_btnWasPressed) {
        _btnPressStart = millis();
        _btnWasPressed = true;
    } else if (!pressed && _btnWasPressed) {
        unsigned long held = millis() - _btnPressStart;
        _btnWasPressed = false;
        if      (held >= BTN_HOLD_RESET_MS)     { NvsConfig::factoryReset(); }
        else if (held >= BTN_HOLD_PROVISION_MS) { WifiManager::startProvisioning(); }
        else                                    { OledDisplay::nextScreen(); }
    }
}
