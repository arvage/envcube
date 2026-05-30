// ============================================================
//  EnvCube — WiFi manager + captive portal implementation
// ============================================================

#include "wifi_manager.h"
#include "../storage/nvs_config.h"
#include "../config.h"
#include <WiFi.h>
#include <WiFiManager.h>    // tzapu/WiFiManager
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include "../display/oled.h"
#include "../outputs/led.h"
#include "../outputs/buzzer.h"

WifiState WifiManager::_state           = WifiState::DISCONNECTED;
unsigned long WifiManager::_lastReconnectAttempt = 0;

// ── Custom WiFiManager parameters ────────────────────────────
// These extra fields appear on the captive portal setup page.
static WiFiManagerParameter param_room_name("room",       "Room name (e.g. Kitchen)",  "EnvCube", 32);
static WiFiManagerParameter param_mqtt_host("mqtt_host",  "MQTT broker IP (optional)", "",        64);
static WiFiManagerParameter param_latitude ("lat",        "Latitude  (e.g. 33.6846)",  "",        12);
static WiFiManagerParameter param_longitude("lon",        "Longitude (e.g. -117.826)", "",        12);

// ── WifiManager::begin ───────────────────────────────────────
void WifiManager::begin() {
    WiFi.mode(WIFI_STA);

    if (g_config.wifi_configured && g_config.is_provisioned) {
        Serial.printf("[WiFi] Connecting to %s ...\n", g_config.wifi_ssid);
        _state = WifiState::CONNECTING;
        _connectWithCredentials();
    } else {
        Serial.println("[WiFi] No credentials — entering provisioning mode");
        _startCaptivePortal();
    }
}

// ── WifiManager::loop ────────────────────────────────────────
void WifiManager::loop() {
    if (_state == WifiState::CONNECTED) {
        ArduinoOTA.handle();
    }

    // Background reconnect if dropped
    if (_state == WifiState::DISCONNECTED || _state == WifiState::FAILED) {
        unsigned long now = millis();
        if (now - _lastReconnectAttempt > WIFI_RETRY_INTERVAL) {
            _lastReconnectAttempt = now;
            if (g_config.wifi_configured) {
                Serial.println("[WiFi] Attempting reconnect...");
                _connectWithCredentials();
            }
        }
    }
}

// ── WifiManager::startProvisioning ───────────────────────────
void WifiManager::startProvisioning() {
    Serial.println("[WiFi] Manual re-provisioning triggered");
    _startCaptivePortal();
}

// ── WifiManager::reconnect ───────────────────────────────────
void WifiManager::reconnect() {
    _state = WifiState::DISCONNECTED;
    _lastReconnectAttempt = 0;
}

// ── WifiManager::state ───────────────────────────────────────
WifiState WifiManager::state() {
    return _state;
}

bool WifiManager::isConnected() {
    return (_state == WifiState::CONNECTED && WiFi.status() == WL_CONNECTED);
}

String WifiManager::ipAddress() {
    if (!isConnected()) return "";
    return WiFi.localIP().toString();
}

int WifiManager::rssi() {
    if (!isConnected()) return 0;
    return WiFi.RSSI();
}

// ── _connectWithCredentials ──────────────────────────────────
void WifiManager::_connectWithCredentials() {
    Serial.printf("[WiFi] Connecting to %s ...\n", g_config.wifi_ssid);
    WiFi.begin(g_config.wifi_ssid, g_config.wifi_password);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        delay(250);
        Serial.print(".");
        if (millis() - start > (WIFI_CONNECT_TIMEOUT * 1000UL)) {
            Serial.println("\n[WiFi] Primary timed out");
            if (strlen(g_config.wifi_ssid2) > 0) {
                Serial.printf("[WiFi] Trying failover: %s ...\n", g_config.wifi_ssid2);
                WiFi.begin(g_config.wifi_ssid2, g_config.wifi_password2);
                unsigned long start2 = millis();
                while (WiFi.status() != WL_CONNECTED) {
                    delay(250);
                    Serial.print(".");
                    if (millis() - start2 > (WIFI_CONNECT_TIMEOUT * 1000UL)) {
                        Serial.println("\n[WiFi] Failover timed out");
                        _state = WifiState::FAILED;
                        return;
                    }
                }
            } else {
                _state = WifiState::FAILED;
                return;
            }
        }
    }

    Serial.printf("\n[WiFi] Connected — IP: %s  RSSI: %d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
    _state = WifiState::CONNECTED;

    // mDNS: room-based hostname for human-friendly web access
    String hostname = String(MDNS_HOSTNAME) + "-" + String(g_config.room_name);
    hostname.toLowerCase();
    hostname.replace(" ", "-");
    if (MDNS.begin(hostname.c_str())) {
        Serial.printf("[mDNS] Hostname: %s.local\n", hostname.c_str());
    }

    // OTA: cube_id-based hostname — stable regardless of room renames or DHCP
    String otaHostname = String(MDNS_HOSTNAME) + "-" + String(g_config.cube_id);
    ArduinoOTA.setPort(OTA_PORT);
    ArduinoOTA.setPassword(OTA_PASSWORD);
    ArduinoOTA.setHostname(otaHostname.c_str());
    Serial.printf("[OTA] Hostname: %s.local\n", otaHostname.c_str());

    ArduinoOTA.onStart([]() {
        Serial.println("[OTA] Starting firmware update...");
        Buzzer::stop();
        Led::flash(LED_COLOR_WHITE, 0);  // infinite white flash
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("\n[OTA] Done — rebooting");
        Led::setColor(LED_COLOR_GREEN);
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("[OTA] Error[%u]\n", error);
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        uint8_t pct = (uint8_t)(progress * 100 / total);
        Serial.printf("[OTA] %u%%\r", pct);
        OledDisplay::showOtaProgress(pct);
    });
    ArduinoOTA.begin();
    Serial.println("[OTA] Ready");
}

// ── _startCaptivePortal ──────────────────────────────────────
void WifiManager::_startCaptivePortal() {
    _state = WifiState::PROVISIONING;

    ::WiFiManager wm;  // tzapu WiFiManager

    // Add our custom fields to the portal page
    wm.addParameter(&param_room_name);
    wm.addParameter(&param_mqtt_host);
    wm.addParameter(&param_latitude);
    wm.addParameter(&param_longitude);

    // Portal appearance
    wm.setTitle("EnvCube Setup");
    wm.setConfigPortalTimeout(300);  // 5-minute timeout
    wm.setAPStaticIPConfig(
        IPAddress(192, 168, 4, 1),
        IPAddress(192, 168, 4, 1),
        IPAddress(255, 255, 255, 0)
    );

    // Save button callback
    wm.setSaveParamsCallback([]() {
        WifiManager::_onProvisionComplete(
            param_room_name.getValue(),
            "",   // WiFiManager handles SSID/pass itself
            param_room_name.getValue(),
            atof(param_latitude.getValue()),
            atof(param_longitude.getValue()),
            param_mqtt_host.getValue()
        );
    });

    Serial.printf("[Portal] Starting AP: %s\n", WIFI_AP_SSID);
    bool connected = wm.startConfigPortal(WIFI_AP_SSID, WIFI_AP_PASSWORD);

    if (connected) {
        Serial.println("[Portal] Provisioning complete — connected");
        // WiFiManager saves SSID/pass to its own NVS — copy to ours
        NvsConfig::saveWifi(WiFi.SSID().c_str(), WiFi.psk().c_str());
        NvsConfig::setProvisioned(true);
        _state = WifiState::CONNECTED;

        // mDNS + OTA setup
        _connectWithCredentials();
    } else {
        Serial.println("[Portal] Portal timed out without credentials");
        _state = WifiState::FAILED;
        // Reboot and try again
        delay(1000);
        ESP.restart();
    }
}

// ── _onProvisionComplete ─────────────────────────────────────
void WifiManager::_onProvisionComplete(const char* ssid,
                                        const char* password,
                                        const char* room_name,
                                        float lat, float lon,
                                        const char* mqtt_host) {
    // Persist all provisioning data
    NvsConfig::saveIdentity(room_name, lat, lon);

    if (strlen(mqtt_host) > 0) {
        NvsConfig::saveMqtt(mqtt_host, MQTT_PORT);
    }

    Serial.printf("[Portal] Saved — room: %s  MQTT: %s\n", room_name, mqtt_host);
}
