#pragma once
// ============================================================
//  EnvCube — NVS configuration store
//  Wraps Arduino Preferences (ESP32 NVS) for persistent config.
//  All values survive power cycles and firmware updates.
// ============================================================

#include <Arduino.h>
#include <Preferences.h>
#include "config.h"

// Max string lengths
#define NVS_MAX_ROOM_NAME     32
#define NVS_MAX_SSID          64
#define NVS_MAX_PASSWORD      64
#define NVS_MAX_MQTT_HOST     64
#define NVS_MAX_MQTT_USER     64
#define NVS_MAX_MQTT_PASS     64
#define NVS_NAMESPACE         "envcube"

// ── Runtime config struct ────────────────────────────────────
// Populated from NVS on boot. All modules read from this.
struct EnvCubeConfig {
    // Identity
    char     room_name[NVS_MAX_ROOM_NAME];  // e.g. "Kitchen"
    uint8_t  cube_id;                       // 0–255, derived from MAC

    // WiFi credentials
    char     wifi_ssid[NVS_MAX_SSID];
    char     wifi_password[NVS_MAX_PASSWORD];
    char     wifi_ssid2[NVS_MAX_SSID];      // failover network
    char     wifi_password2[NVS_MAX_PASSWORD];
    bool     wifi_configured;

    // MQTT
    char     mqtt_host[NVS_MAX_MQTT_HOST];
    uint16_t mqtt_port;
    bool     mqtt_enabled;
    char     mqtt_user[NVS_MAX_MQTT_USER];
    char     mqtt_password[NVS_MAX_MQTT_PASS];

    // Location for weather (set during provisioning)
    float    latitude;
    float    longitude;

    // Alert thresholds (can be tuned per-cube via MQTT)
    float    thresh_temp_warn;
    float    thresh_temp_alert;
    uint16_t thresh_co2_warn;
    uint16_t thresh_co2_alert;
    uint16_t thresh_smoke_warn;
    uint16_t thresh_smoke_alert;
    uint8_t  thresh_pm25_warn;
    uint8_t  thresh_pm25_alert;
    uint8_t  thresh_noise_warn;
    uint8_t  thresh_noise_alert;

    // Behaviour flags
    bool     presence_suppress_alerts;  // suppress non-critical when empty
    bool     buzzer_enabled;
    bool     voice_enabled;
    uint8_t  led_brightness;

    // Provisioned flag
    bool     is_provisioned;
};

// ── Public API ───────────────────────────────────────────────
class NvsConfig {
public:
    // Load config from NVS into g_config. Call once at boot.
    static bool load();

    // Save full config struct to NVS.
    static bool save();

    // Save only WiFi credentials (called from captive portal).
    static bool saveWifi(const char* ssid, const char* password);

    // Save room name + location (called from captive portal).
    static bool saveIdentity(const char* room_name, float lat, float lon);

    // Save MQTT settings.
    static bool saveMqtt(const char* host, uint16_t port);

    // Save individual threshold (called via MQTT command).
    static bool saveThreshold(const char* key, float value);

    // Mark device as provisioned.
    static bool setProvisioned(bool provisioned);

    // Reset all stored config to defaults (factory reset).
    static bool factoryReset();

    // Print current config to Serial (for debugging).
    static void dump();
};

// ── Global config instance ───────────────────────────────────
// All modules include this header and access g_config directly.
extern EnvCubeConfig g_config;
