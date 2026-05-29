// ============================================================
//  EnvCube — main.cpp
//  Boot sequence, FreeRTOS task launch.
//  Phase 1: NVS config → WiFi → Sensors → Alerts → Display
// ============================================================

#include <Arduino.h>
#include "config.h"
#include "storage/nvs_config.h"
#include "connectivity/wifi_manager.h"
#include "alerts/alert_engine.h"
#include "outputs/led.h"
#include "outputs/buzzer.h"
#include "outputs/dfplayer.h"
#include "display/oled.h"

// ── Forward declarations ──────────────────────────────────────
void taskSensors(void* param);
void taskDisplay(void* param);
void taskConnectivity(void* param);
void handleButton();

// ── Button state ─────────────────────────────────────────────
static unsigned long _btnPressStart = 0;
static bool          _btnWasPressed = false;

// ── setup ─────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("\n╔═══════════════════════════╗");
    Serial.println("║  EnvCube v" ENVCUBE_VERSION "           ║");
    Serial.println("║  Phase " + String(ENVCUBE_BUILD_PHASE) + " prototype          ║");
    Serial.println("╚═══════════════════════════╝");

    // ── 1. GPIO setup ─────────────────────────────────────────
    pinMode(PIN_BUTTON, INPUT_PULLUP);

    // ── 2. LED — show boot colour immediately ─────────────────
    Led::begin();
    Led::setColor(LED_COLOR_WHITE);  // White = booting

    // ── 3. Load NVS config ────────────────────────────────────
    Serial.println("[Boot] Loading NVS config...");
    NvsConfig::load();
    NvsConfig::dump();

    // ── 4. Alert engine ───────────────────────────────────────
    AlertEngine::begin();

    // ── 5. Buzzer + DFPlayer ──────────────────────────────────
    Buzzer::begin();
    DFPlayer::begin();

    // ── 6. OLED display ───────────────────────────────────────
    OledDisplay::begin();
    OledDisplay::showBoot(ENVCUBE_VERSION);

    // ── 7. WiFi — blocks until connected or provisioning done ─
    Serial.println("[Boot] Starting WiFi manager...");
    if (!g_config.is_provisioned) {
        Led::pulse(LED_COLOR_BLUE);   // Blue pulse = provisioning mode
        OledDisplay::showProvisioning(WIFI_AP_SSID);
    } else {
        Led::pulse(LED_COLOR_WHITE);  // White pulse = connecting
    }
    WifiManager::begin();

    if (WifiManager::isConnected()) {
        Led::setColor(LED_COLOR_GREEN);
        Serial.printf("[Boot] WiFi connected — %s\n", WifiManager::ipAddress().c_str());
        if (g_config.voice_enabled) DFPlayer::play(AUDIO_WIFI_CONNECTED);
    }

    // ── 8. Launch FreeRTOS tasks ──────────────────────────────
    // Sensor + alert task — high priority, runs on core 0
    xTaskCreatePinnedToCore(
        taskSensors, "sensors",
        8192, nullptr, 3, nullptr, 0
    );

    // Display update task — medium priority
    xTaskCreatePinnedToCore(
        taskDisplay, "display",
        4096, nullptr, 2, nullptr, 1
    );

    // Connectivity task (MQTT, weather, heartbeat) — low priority
    xTaskCreatePinnedToCore(
        taskConnectivity, "connectivity",
        8192, nullptr, 1, nullptr, 1
    );

    Serial.println("[Boot] All tasks launched — EnvCube running");
    Led::setAlert(AlertLevel::ALL_CLEAR);  // Green = all good
}

// ── loop ──────────────────────────────────────────────────────
// Main loop handles WiFi reconnect, OTA, and button only.
// All sensor/display work is in FreeRTOS tasks.
void loop() {
    WifiManager::loop();  // ArduinoOTA + reconnect
    handleButton();
    delay(50);
}

// ── taskSensors ───────────────────────────────────────────────
// Reads all sensors and runs alert evaluation every POLL_FAST_MS.
// Includes MQ-2 warm-up guard on first run.
void taskSensors(void* param) {
    // Import sensor drivers (lazy include — keeps main.cpp clean)
    #include "sensors/sht40.h"
    #include "sensors/scd41.h"
    #include "sensors/sgp41.h"
    #include "sensors/bmp280.h"
    #include "sensors/veml7700.h"
    #include "sensors/mq2.h"
    #include "sensors/pmsa003i.h"
    #include "sensors/ld2410c.h"
    #include "sensors/ics43434.h"

    // Initialise all sensor drivers
    Sht40::begin();
    Scd41::begin();
    Sgp41::begin();
    Bmp280Driver::begin();
    Veml7700Driver::begin();
    Mq2::begin();
    Pmsa003i::begin();
    Ld2410c::begin();
    Ics43434::begin();

    Serial.println("[Sensors] All initialised — starting poll loop");

    // MQ-2 warm-up: ~60 s on first boot
    Serial.printf("[Sensors] MQ-2 warm-up %u s...\n", MQ2_WARMUP_MS / 1000);
    vTaskDelay(pdMS_TO_TICKS(MQ2_WARMUP_MS));
    Serial.println("[Sensors] MQ-2 warm-up complete");

    unsigned long lastSlowPoll = 0;

    for (;;) {
        unsigned long now = millis();

        // ── Fast sensors (every POLL_FAST_MS) ────────────────
        Sht40::read(g_readings);
        Mq2::read(g_readings);
        Ld2410c::read(g_readings);
        Pmsa003i::read(g_readings);
        Ics43434::read(g_readings);
        Veml7700Driver::read(g_readings);
        Bmp280Driver::read(g_readings);

        // ── Slow sensors (every POLL_SLOW_MS) ────────────────
        if (now - lastSlowPoll >= POLL_SLOW_MS) {
            Scd41::read(g_readings);
            // SGP41 needs SHT40 data for humidity compensation
            Sgp41::read(g_readings, g_readings.temperature_c,
                                    g_readings.humidity_rh);
            lastSlowPoll = now;
        }

        g_readings.last_update_ms = millis();

        // Evaluate all readings against thresholds
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
        vTaskDelay(pdMS_TO_TICKS(2000));  // Refresh every 2 s
    }
}

// ── taskConnectivity ──────────────────────────────────────────
void taskConnectivity(void* param) {
    // Lazy imports — only compiled when flags are set
    #ifdef ENVCUBE_ENABLE_MQTT
    #include "connectivity/mqtt_client.h"
    MqttClient::begin();
    #endif

    #ifdef ENVCUBE_ENABLE_WEATHER
    #include "connectivity/weather.h"
    #endif

    unsigned long lastHeartbeat = 0;
    unsigned long lastWeather   = 0;

    for (;;) {
        if (!WifiManager::isConnected()) {
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        unsigned long now = millis();

        #ifdef ENVCUBE_ENABLE_MQTT
        MqttClient::loop();
        // Publish sensor readings
        if (now % 10000 < 50) {  // ~every 10 s
            MqttClient::publishReadings(g_readings);
            MqttClient::publishAlertState(AlertEngine::currentLevel(),
                                          AlertEngine::currentMessage());
        }
        #endif

        // Heartbeat (cloud watchdog)
        if (now - lastHeartbeat >= HEARTBEAT_INTERVAL_MS) {
            #ifdef ENVCUBE_ENABLE_MQTT
            MqttClient::publishHeartbeat();
            #endif
            lastHeartbeat = now;
        }

        // Weather update
        #ifdef ENVCUBE_ENABLE_WEATHER
        if (now - lastWeather >= WEATHER_UPDATE_MS) {
            Weather::fetch(g_config.latitude, g_config.longitude);
            lastWeather = now;
        }
        #endif

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

        if (held >= BTN_HOLD_RESET_MS) {
            Serial.println("[BTN] Factory reset triggered");
            NvsConfig::factoryReset();  // reboots
        } else if (held >= BTN_HOLD_PROVISION_MS) {
            Serial.println("[BTN] Re-provisioning triggered");
            WifiManager::startProvisioning();
        } else {
            // Short tap — cycle OLED screen
            OledDisplay::nextScreen();
        }
    }
}
