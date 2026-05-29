// ============================================================
//  EnvCube — NVS configuration store implementation
// ============================================================

#include "nvs_config.h"

// Global config instance — all modules read this
EnvCubeConfig g_config = {};

static Preferences _prefs;

// ── Helpers ──────────────────────────────────────────────────
static void _applyDefaults() {
    strncpy(g_config.room_name,  "EnvCube",      NVS_MAX_ROOM_NAME - 1);
    g_config.wifi_configured     = false;
    g_config.mqtt_port           = MQTT_PORT;
    g_config.mqtt_enabled        = false;
    g_config.latitude            = 0.0f;
    g_config.longitude           = 0.0f;
    g_config.thresh_temp_warn    = THRESH_TEMP_WARN;
    g_config.thresh_temp_alert   = THRESH_TEMP_ALERT;
    g_config.thresh_co2_warn     = THRESH_CO2_WARN;
    g_config.thresh_co2_alert    = THRESH_CO2_ALERT;
    g_config.thresh_smoke_warn   = THRESH_SMOKE_WARN;
    g_config.thresh_smoke_alert  = THRESH_SMOKE_ALERT;
    g_config.thresh_pm25_warn    = THRESH_PM25_WARN;
    g_config.thresh_pm25_alert   = THRESH_PM25_ALERT;
    g_config.thresh_noise_warn   = THRESH_NOISE_WARN;
    g_config.thresh_noise_alert  = THRESH_NOISE_ALERT;
    g_config.presence_suppress_alerts = true;
    g_config.buzzer_enabled      = true;
    g_config.voice_enabled       = true;
    g_config.led_brightness      = LED_BRIGHTNESS;
    g_config.is_provisioned      = false;

    // Derive cube_id from last byte of MAC
    // WiFi.macAddress() works on all Arduino ESP32 Core versions
    uint8_t mac[6];
    WiFi.macAddress(mac);
    g_config.cube_id = mac[5];
}

// ── NvsConfig::load ──────────────────────────────────────────
bool NvsConfig::load() {
    _applyDefaults();

    if (!_prefs.begin(NVS_NAMESPACE, true)) {  // true = read-only
        Serial.println("[NVS] No namespace found, using defaults");
        return false;
    }

    g_config.is_provisioned = _prefs.getBool("provisioned", false);

    if (g_config.is_provisioned) {
        _prefs.getString("room_name",      g_config.room_name,     NVS_MAX_ROOM_NAME);
        _prefs.getString("wifi_ssid",      g_config.wifi_ssid,     NVS_MAX_SSID);
        _prefs.getString("wifi_password",  g_config.wifi_password, NVS_MAX_PASSWORD);
        g_config.wifi_configured = (strlen(g_config.wifi_ssid) > 0);

        _prefs.getString("mqtt_host",      g_config.mqtt_host,     NVS_MAX_MQTT_HOST);
        g_config.mqtt_port    = _prefs.getUShort("mqtt_port",     MQTT_PORT);
        g_config.mqtt_enabled = _prefs.getBool("mqtt_enabled",    false);

        g_config.latitude  = _prefs.getFloat("latitude",  0.0f);
        g_config.longitude = _prefs.getFloat("longitude", 0.0f);

        // Thresholds
        g_config.thresh_temp_warn   = _prefs.getFloat("thr_tmp_w",  THRESH_TEMP_WARN);
        g_config.thresh_temp_alert  = _prefs.getFloat("thr_tmp_a",  THRESH_TEMP_ALERT);
        g_config.thresh_co2_warn    = _prefs.getUShort("thr_co2_w", THRESH_CO2_WARN);
        g_config.thresh_co2_alert   = _prefs.getUShort("thr_co2_a", THRESH_CO2_ALERT);
        g_config.thresh_smoke_warn  = _prefs.getUShort("thr_smk_w", THRESH_SMOKE_WARN);
        g_config.thresh_smoke_alert = _prefs.getUShort("thr_smk_a", THRESH_SMOKE_ALERT);
        g_config.thresh_pm25_warn   = _prefs.getUChar("thr_pm_w",   THRESH_PM25_WARN);
        g_config.thresh_pm25_alert  = _prefs.getUChar("thr_pm_a",   THRESH_PM25_ALERT);
        g_config.thresh_noise_warn  = _prefs.getUChar("thr_nz_w",   THRESH_NOISE_WARN);
        g_config.thresh_noise_alert = _prefs.getUChar("thr_nz_a",   THRESH_NOISE_ALERT);

        // Behaviour
        g_config.presence_suppress_alerts = _prefs.getBool("pres_suppress", true);
        g_config.buzzer_enabled = _prefs.getBool("buzzer_en", true);
        g_config.voice_enabled  = _prefs.getBool("voice_en",  true);
        g_config.led_brightness = _prefs.getUChar("led_brt",  LED_BRIGHTNESS);
    }

    _prefs.end();
    Serial.printf("[NVS] Loaded — room: %s  provisioned: %s\n",
                  g_config.room_name,
                  g_config.is_provisioned ? "yes" : "no");
    return true;
}

// ── NvsConfig::save ──────────────────────────────────────────
bool NvsConfig::save() {
    if (!_prefs.begin(NVS_NAMESPACE, false)) {  // false = read-write
        Serial.println("[NVS] ERROR: could not open namespace for writing");
        return false;
    }

    _prefs.putBool("provisioned",  g_config.is_provisioned);
    _prefs.putString("room_name",  g_config.room_name);
    _prefs.putString("wifi_ssid",  g_config.wifi_ssid);
    _prefs.putString("wifi_password", g_config.wifi_password);
    _prefs.putString("mqtt_host",  g_config.mqtt_host);
    _prefs.putUShort("mqtt_port",  g_config.mqtt_port);
    _prefs.putBool("mqtt_enabled", g_config.mqtt_enabled);
    _prefs.putFloat("latitude",    g_config.latitude);
    _prefs.putFloat("longitude",   g_config.longitude);

    _prefs.putFloat("thr_tmp_w",   g_config.thresh_temp_warn);
    _prefs.putFloat("thr_tmp_a",   g_config.thresh_temp_alert);
    _prefs.putUShort("thr_co2_w",  g_config.thresh_co2_warn);
    _prefs.putUShort("thr_co2_a",  g_config.thresh_co2_alert);
    _prefs.putUShort("thr_smk_w",  g_config.thresh_smoke_warn);
    _prefs.putUShort("thr_smk_a",  g_config.thresh_smoke_alert);
    _prefs.putUChar("thr_pm_w",    g_config.thresh_pm25_warn);
    _prefs.putUChar("thr_pm_a",    g_config.thresh_pm25_alert);
    _prefs.putUChar("thr_nz_w",    g_config.thresh_noise_warn);
    _prefs.putUChar("thr_nz_a",    g_config.thresh_noise_alert);

    _prefs.putBool("pres_suppress", g_config.presence_suppress_alerts);
    _prefs.putBool("buzzer_en",     g_config.buzzer_enabled);
    _prefs.putBool("voice_en",      g_config.voice_enabled);
    _prefs.putUChar("led_brt",      g_config.led_brightness);

    _prefs.end();
    Serial.println("[NVS] Config saved");
    return true;
}

// ── NvsConfig::saveWifi ──────────────────────────────────────
bool NvsConfig::saveWifi(const char* ssid, const char* password) {
    strncpy(g_config.wifi_ssid,     ssid,     NVS_MAX_SSID - 1);
    strncpy(g_config.wifi_password, password, NVS_MAX_PASSWORD - 1);
    g_config.wifi_configured = true;
    return save();
}

// ── NvsConfig::saveIdentity ──────────────────────────────────
bool NvsConfig::saveIdentity(const char* room_name, float lat, float lon) {
    strncpy(g_config.room_name, room_name, NVS_MAX_ROOM_NAME - 1);
    g_config.latitude  = lat;
    g_config.longitude = lon;
    return save();
}

// ── NvsConfig::saveMqtt ──────────────────────────────────────
bool NvsConfig::saveMqtt(const char* host, uint16_t port) {
    strncpy(g_config.mqtt_host, host, NVS_MAX_MQTT_HOST - 1);
    g_config.mqtt_port    = port;
    g_config.mqtt_enabled = (strlen(host) > 0);
    return save();
}

// ── NvsConfig::saveThreshold ─────────────────────────────────
bool NvsConfig::saveThreshold(const char* key, float value) {
    if (!_prefs.begin(NVS_NAMESPACE, false)) return false;
    _prefs.putFloat(key, value);
    _prefs.end();
    return true;
}

// ── NvsConfig::setProvisioned ────────────────────────────────
bool NvsConfig::setProvisioned(bool provisioned) {
    g_config.is_provisioned = provisioned;
    if (!_prefs.begin(NVS_NAMESPACE, false)) return false;
    _prefs.putBool("provisioned", provisioned);
    _prefs.end();
    return true;
}

// ── NvsConfig::factoryReset ──────────────────────────────────
bool NvsConfig::factoryReset() {
    Serial.println("[NVS] FACTORY RESET — clearing all stored config");
    if (!_prefs.begin(NVS_NAMESPACE, false)) return false;
    _prefs.clear();
    _prefs.end();
    _applyDefaults();
    Serial.println("[NVS] Factory reset complete — rebooting in 2 s");
    delay(2000);
    ESP.restart();
    return true;
}

// ── NvsConfig::dump ──────────────────────────────────────────
void NvsConfig::dump() {
    Serial.println("──── EnvCube NVS Config ────");
    Serial.printf("  room_name:    %s\n",  g_config.room_name);
    Serial.printf("  cube_id:      %u\n",  g_config.cube_id);
    Serial.printf("  provisioned:  %s\n",  g_config.is_provisioned ? "yes" : "no");
    Serial.printf("  wifi_ssid:    %s\n",  g_config.wifi_ssid);
    Serial.printf("  mqtt_host:    %s:%u\n", g_config.mqtt_host, g_config.mqtt_port);
    Serial.printf("  location:     %.4f, %.4f\n", g_config.latitude, g_config.longitude);
    Serial.printf("  thresholds:\n");
    Serial.printf("    temp  warn/alert: %.1f / %.1f °C\n",   g_config.thresh_temp_warn, g_config.thresh_temp_alert);
    Serial.printf("    CO₂   warn/alert: %u / %u ppm\n",      g_config.thresh_co2_warn,  g_config.thresh_co2_alert);
    Serial.printf("    smoke warn/alert: %u / %u (raw)\n",    g_config.thresh_smoke_warn, g_config.thresh_smoke_alert);
    Serial.printf("    PM2.5 warn/alert: %u / %u µg/m³\n",   g_config.thresh_pm25_warn, g_config.thresh_pm25_alert);
    Serial.printf("    noise warn/alert: %u / %u dB(A)\n",   g_config.thresh_noise_warn, g_config.thresh_noise_alert);
    Serial.println("────────────────────────────");
}