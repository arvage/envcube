// ============================================================
//  EnvCube — MQTT client + Home Assistant auto-discovery
//
//  PubSubClient handles MQTT protocol.
//  ArduinoJson builds payloads.
//
//  HA auto-discovery:
//  On first connect we publish a config payload for every
//  sensor entity. HA reads these once and creates entities
//  automatically — no YAML needed.
//
//  Last Will Testament (LWT):
//  Broker publishes envcube/<room>/status offline payload
//  automatically if the cube disconnects unexpectedly.
//  This is the cloud watchdog — HA automations can trigger
//  on this to send phone notifications.
// ============================================================

#include "mqtt_client.h"
#include "../config.h"
#include "../storage/nvs_config.h"
#include "../alerts/alert_engine.h"
#include "../web/logger.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

static WiFiClient   _wifiClient;
static PubSubClient _mqtt(_wifiClient);

bool     MqttClient::_connected          = false;
bool     MqttClient::_discoveryPublished = false;
uint32_t MqttClient::_lastReconnectMs    = 0;
uint32_t MqttClient::_lastPublishMs      = 0;

// ── MqttClient::begin ────────────────────────────────────────
void MqttClient::begin() {
    if (!g_config.mqtt_enabled || strlen(g_config.mqtt_host) == 0) {
        Serial.println("[MQTT] Disabled — no broker configured");
        return;
    }

    _mqtt.setServer(g_config.mqtt_host, g_config.mqtt_port);
    _mqtt.setCallback(_onMessage);
    _mqtt.setKeepAlive(MQTT_KEEPALIVE);
    _mqtt.setBufferSize(2048);

    Logger::write('I', "MQTT", "Configured — broker: %s:%u user: %s",
                  g_config.mqtt_host, g_config.mqtt_port,
                  strlen(g_config.mqtt_user) > 0 ? g_config.mqtt_user : "(none)");
    _connect();
}

// ── MqttClient::loop ─────────────────────────────────────────
void MqttClient::loop() {
    if (!g_config.mqtt_enabled) return;

    if (!_mqtt.connected()) {
        _connected = false;
        uint32_t now = millis();
        if (now - _lastReconnectMs >= RECONNECT_INTERVAL_MS) {
            _lastReconnectMs = now;
            _connect();
        }
        return;
    }

    _connected = true;
    _mqtt.loop();
}

// ── MqttClient::publishReadings ──────────────────────────────
void MqttClient::publishReadings(const SensorReadings& r) {
    if (!_connected) return;

    char topic[80];
    char payload[128];
    uint8_t published = 0;

    if (r.thermal_ok) {
        _topic(topic, sizeof(topic), "temperature");
        snprintf(payload, sizeof(payload),
                 "{\"value\":%.2f,\"unit\":\"C\"}", r.temperature_c);
        _mqtt.publish(topic, payload, true); published++;

        _topic(topic, sizeof(topic), "humidity");
        snprintf(payload, sizeof(payload),
                 "{\"value\":%.1f,\"unit\":\"%%\"}", r.humidity_rh);
        _mqtt.publish(topic, payload, true); published++;
    }

    if (r.pressure_ok) {
        _topic(topic, sizeof(topic), "pressure");
        snprintf(payload, sizeof(payload),
                 "{\"value\":%.1f,\"unit\":\"hPa\"}", r.pressure_hpa);
        _mqtt.publish(topic, payload, true); published++;
    }

    if (r.co2_ok) {
        _topic(topic, sizeof(topic), "co2");
        snprintf(payload, sizeof(payload),
                 "{\"value\":%u,\"unit\":\"ppm\"}", r.co2_ppm);
        _mqtt.publish(topic, payload, true); published++;
    }

    if (r.smoke_ok) {
        bool detected = r.smoke_raw >= g_config.thresh_smoke_warn;
        _topic(topic, sizeof(topic), "smoke");
        snprintf(payload, sizeof(payload),
                 "{\"value\":%s,\"raw\":%u}",
                 detected ? "true" : "false", r.smoke_raw);
        _mqtt.publish(topic, payload, true); published++;
    }

    if (r.aq_ok) {
        _topic(topic, sizeof(topic), "voc_index");
        snprintf(payload, sizeof(payload), "{\"value\":%d}", r.voc_index);
        _mqtt.publish(topic, payload, true); published++;

        _topic(topic, sizeof(topic), "nox_index");
        snprintf(payload, sizeof(payload), "{\"value\":%d}", r.nox_index);
        _mqtt.publish(topic, payload, true); published++;
    }

    // Lux uses its own flag — independent of SGP41 (aq_ok)
    if (r.lux_ok) {
        _topic(topic, sizeof(topic), "lux");
        snprintf(payload, sizeof(payload),
                 "{\"value\":%lu,\"unit\":\"lx\"}", (unsigned long)r.lux);
        _mqtt.publish(topic, payload, true); published++;
    }

    if (r.pm_ok) {
        _topic(topic, sizeof(topic), "pm25");
        snprintf(payload, sizeof(payload),
                 "{\"value\":%u,\"unit\":\"ug/m3\"}", r.pm2_5);
        _mqtt.publish(topic, payload, true); published++;

        _topic(topic, sizeof(topic), "pm10");
        snprintf(payload, sizeof(payload),
                 "{\"value\":%u,\"unit\":\"ug/m3\"}", r.pm10);
        _mqtt.publish(topic, payload, true); published++;
    }

    if (r.noise_ok) {
        _topic(topic, sizeof(topic), "noise_db");
        snprintf(payload, sizeof(payload),
                 "{\"value\":%.1f,\"unit\":\"dBA\"}", r.noise_db);
        _mqtt.publish(topic, payload, true); published++;
    }

    // Presence
    if (r.presence_ok) {
        _topic(topic, sizeof(topic), "presence");
        snprintf(payload, sizeof(payload),
                 "{\"value\":%s,\"distance_cm\":%u}",
                 r.presence ? "true" : "false", r.presence_cm);
        _mqtt.publish(topic, payload, true); published++;
    }

    Logger::write('I', "MQTT", "Published %u sensor(s)", published);
}

// ── MqttClient::publishAlert ─────────────────────────────────
void MqttClient::publishAlert(AlertLevel level, AlertSource source,
                               const char* message) {
    if (!_connected) return;

    const char* sourceStr = "none";
    switch (source) {
        case AlertSource::SMOKE:       sourceStr = "smoke";       break;
        case AlertSource::CO2:         sourceStr = "co2";         break;
        case AlertSource::TEMPERATURE: sourceStr = "temperature"; break;
        case AlertSource::HUMIDITY:    sourceStr = "humidity";    break;
        case AlertSource::VOC:         sourceStr = "voc";         break;
        case AlertSource::NOX:         sourceStr = "nox";         break;
        case AlertSource::PM25:        sourceStr = "pm25";        break;
        case AlertSource::NOISE:       sourceStr = "noise";       break;
        case AlertSource::REMOTE:      sourceStr = "remote";      break;
        default:                       sourceStr = "none";        break;
    }

    char topic[80], payload[200];
    _topic(topic, sizeof(topic), "alert");
    snprintf(payload, sizeof(payload),
             "{\"level\":%d,\"source\":\"%s\",\"message\":\"%s\"}",
             (int)level, sourceStr, message);
    _mqtt.publish(topic, payload, true);
}

// ── MqttClient::publishHeartbeat ─────────────────────────────
void MqttClient::publishHeartbeat() {
    if (!_connected) return;
    char topic[80], payload[64];
    _topic(topic, sizeof(topic), "heartbeat");
    snprintf(payload, sizeof(payload),
             "{\"ts\":%lu,\"cube_id\":%u}",
             millis() / 1000, g_config.cube_id);
    _mqtt.publish(topic, payload);  // not retained — watchdog needs live pings
}

// ── MqttClient::publishStatus ────────────────────────────────
void MqttClient::publishStatus(bool online, int rssi,
                                const char* ip, uint32_t uptime_s) {
    if (!_mqtt.connected() && online) return;
    char topic[80], payload[200];
    _topic(topic, sizeof(topic), "status");
    snprintf(payload, sizeof(payload),
             "{\"online\":%s,\"rssi\":%d,\"ip\":\"%s\","
             "\"uptime\":%lu,\"version\":\"%s\"}",
             online ? "true" : "false",
             rssi, ip, (unsigned long)uptime_s,
             ENVCUBE_VERSION);
    _mqtt.publish(topic, payload, true);
}

bool MqttClient::isConnected() { return _connected; }

// ── _connect ─────────────────────────────────────────────────
void MqttClient::_connect() {
    if (!g_config.mqtt_enabled) return;

    // Client ID: envcube-<room>-<cube_id>
    char clientId[48];
    char slug[32];
    _roomSlug(slug, sizeof(slug));
    snprintf(clientId, sizeof(clientId), "envcube-%s-%u",
             slug, g_config.cube_id);

    // LWT — broker will publish this if we disconnect ungracefully
    char lwtTopic[80];
    _topic(lwtTopic, sizeof(lwtTopic), "status");
    char lwtPayload[64];
    snprintf(lwtPayload, sizeof(lwtPayload),
             "{\"online\":false,\"cube_id\":%u}", g_config.cube_id);

    Serial.printf("[MQTT] Connecting as %s ...\n", clientId);

    const char* mqttUser = strlen(g_config.mqtt_user)     > 0 ? g_config.mqtt_user     : nullptr;
    const char* mqttPass = strlen(g_config.mqtt_password) > 0 ? g_config.mqtt_password : nullptr;

    bool ok = _mqtt.connect(clientId,
                            mqttUser, mqttPass,
                            lwtTopic, 1,               // LWT QoS 1
                            true,                      // LWT retained
                            lwtPayload,
                            true);                     // clean session

    if (ok) {
        _connected          = true;
        _discoveryPublished = false;
        Logger::write('I', "MQTT", "Connected to %s", g_config.mqtt_host);

        char cmdTopic[80];
        _topic(cmdTopic, sizeof(cmdTopic), "cmd/#");
        _mqtt.subscribe(cmdTopic);

        publishStatus(true, WiFi.RSSI(),
                      WiFi.localIP().toString().c_str(),
                      millis() / 1000);

        _publishDiscovery();
        _discoveryPublished = true;
    } else {
        // PubSubClient state codes: -4=timeout -3=lost -2=failed
        // 1=bad protocol 2=bad client_id 3=unavailable 4=bad credentials 5=unauthorized
        Logger::write('E', "MQTT", "Connect failed — state: %d (broker: %s user: %s)",
                      _mqtt.state(), g_config.mqtt_host,
                      strlen(g_config.mqtt_user) > 0 ? g_config.mqtt_user : "none");
    }
}

// ── _onMessage ───────────────────────────────────────────────
void MqttClient::_onMessage(char* topic, byte* payload,
                             unsigned int length) {
    // Null-terminate payload
    char msg[128] = {};
    size_t n = (length < sizeof(msg)-1) ? length : sizeof(msg)-1;
    memcpy(msg, payload, n);

    Serial.printf("[MQTT] Received: %s = %s\n", topic, msg);

    // Simple command handler
    // envcube/<room>/cmd/reboot  → reboot
    // envcube/<room>/cmd/identify → flash LED 5 s
    if (strstr(topic, "/cmd/reboot")) {
        Serial.println("[MQTT] Reboot commanded");
        delay(500);
        ESP.restart();
    }
    // Additional commands handled in future steps
}

// ── _publishDiscovery ────────────────────────────────────────
void MqttClient::_clearDiscovery() {
    // Delete old retained discovery entries so HA re-creates entities with
    // correct device grouping. Empty payload = HA removes the entity.
    char slug[32];
    _roomSlug(slug, sizeof(slug));

    const char* ids[] = {
        "temperature","humidity","pressure","co2","voc_index","nox_index",
        "pm25","pm10","noise_db","lux","alert_level"
    };
    const char* binIds[] = { "smoke","presence" };

    char topic[128];
    for (auto id : ids) {
        snprintf(topic, sizeof(topic), "%s/sensor/envcube_%s_%s/config",
                 MQTT_HA_DISCOVERY, slug, id);
        _mqtt.publish(topic, "", true);   // empty = delete
    }
    for (auto id : binIds) {
        snprintf(topic, sizeof(topic), "%s/binary_sensor/envcube_%s_%s/config",
                 MQTT_HA_DISCOVERY, slug, id);
        _mqtt.publish(topic, "", true);
    }
    Logger::write('I', "MQTT", "Cleared old discovery entries");
    delay(200);  // give broker time to process deletes before re-publishing
}

void MqttClient::_publishDiscovery() {
    _clearDiscovery();
    Logger::write('I', "MQTT", "Publishing HA auto-discovery...");

    char slug[32];
    _roomSlug(slug, sizeof(slug));
    char stateTopic[80];

    // ── Numeric sensors ──────────────────────────────────────
    _topic(stateTopic, sizeof(stateTopic), "temperature");
    _publishSensor("temperature", "Temperature", "°C",
                   "temperature", stateTopic, "{{value_json.value}}");

    _topic(stateTopic, sizeof(stateTopic), "humidity");
    _publishSensor("humidity", "Humidity", "%",
                   "humidity", stateTopic, "{{value_json.value}}");

    _topic(stateTopic, sizeof(stateTopic), "pressure");
    _publishSensor("pressure", "Pressure", "hPa",
                   "pressure", stateTopic, "{{value_json.value}}");

    _topic(stateTopic, sizeof(stateTopic), "co2");
    _publishSensor("co2", "CO2", "ppm",
                   "carbon_dioxide", stateTopic, "{{value_json.value}}");

    _topic(stateTopic, sizeof(stateTopic), "voc_index");
    _publishSensor("voc_index", "VOC Index", "",
                   "volatile_organic_compounds_parts",
                   stateTopic, "{{value_json.value}}");

    _topic(stateTopic, sizeof(stateTopic), "nox_index");
    _publishSensor("nox_index", "NOx Index", "",
                   "nitrogen_monoxide",
                   stateTopic, "{{value_json.value}}");

    _topic(stateTopic, sizeof(stateTopic), "pm25");
    _publishSensor("pm25", "PM2.5", "µg/m³",
                   "pm25", stateTopic, "{{value_json.value}}");

    _topic(stateTopic, sizeof(stateTopic), "pm10");
    _publishSensor("pm10", "PM10", "µg/m³",
                   "pm10", stateTopic, "{{value_json.value}}");

    _topic(stateTopic, sizeof(stateTopic), "noise_db");
    _publishSensor("noise_db", "Noise Level", "dB(A)",
                   "sound_pressure", stateTopic, "{{value_json.value}}");

    _topic(stateTopic, sizeof(stateTopic), "lux");
    _publishSensor("lux", "Illuminance", "lx",
                   "illuminance", stateTopic, "{{value_json.value}}");

    // ── Binary sensors ───────────────────────────────────────
    _topic(stateTopic, sizeof(stateTopic), "smoke");
    _publishSensor("smoke", "Smoke", "",
                   "smoke", stateTopic,
                   "{{ 'on' if value_json.value else 'off' }}", true);

    _topic(stateTopic, sizeof(stateTopic), "presence");
    _publishSensor("presence", "Presence", "",
                   "occupancy", stateTopic,
                   "{{ 'on' if value_json.value else 'off' }}", true);

    // ── Alert level sensor ───────────────────────────────────
    _topic(stateTopic, sizeof(stateTopic), "alert");
    _publishSensor("alert_level", "Alert Level", "",
                   nullptr, stateTopic, "{{value_json.level}}");

    Logger::write('I', "MQTT", "HA auto-discovery complete");
}

// ── _publishSensor ───────────────────────────────────────────
void MqttClient::_publishSensor(const char* sensor_id,
                                 const char* name,
                                 const char* unit,
                                 const char* device_class,
                                 const char* state_topic,
                                 const char* value_template,
                                 bool binary) {
    char slug[32];
    _roomSlug(slug, sizeof(slug));

    // Unique entity ID: envcube_<room>_<sensor_id>
    char uid[64];
    snprintf(uid, sizeof(uid), "envcube_%s_%s", slug, sensor_id);

    // Friendly name: "<RoomName> <SensorName>"
    char friendlyName[64];
    snprintf(friendlyName, sizeof(friendlyName), "%s %s",
             g_config.room_name, name);

    // Discovery topic
    char discTopic[120];
    snprintf(discTopic, sizeof(discTopic),
             "%s/%s/%s/config",
             MQTT_HA_DISCOVERY,
             binary ? "binary_sensor" : "sensor",
             uid);

    // Build JSON payload using ArduinoJson
    JsonDocument doc;
    doc["name"]           = friendlyName;
    doc["unique_id"]      = uid;
    doc["state_topic"]    = state_topic;
    doc["value_template"] = value_template;
    if (strlen(unit) > 0)
        doc["unit_of_measurement"] = unit;
    if (device_class != nullptr)
        doc["device_class"] = device_class;
    doc["availability_topic"]  = "";  // set below
    doc["payload_available"]   = "online";
    doc["payload_not_available"] = "offline";

    // Device block — shared across all entities so HA groups them under one device
    char deviceId[48];
    snprintf(deviceId, sizeof(deviceId), "envcube_%s_%u", slug, g_config.cube_id);

    JsonObject device = doc["device"].to<JsonObject>();
    device["identifiers"].to<JsonArray>().add(deviceId);
    device["name"]            = g_config.room_name;
    device["model"]           = "EnvCube";
    device["manufacturer"]    = "EnvCube";
    device["sw_version"]      = ENVCUBE_VERSION;

    // Availability via LWT status topic
    char availTopic[80];
    _topic(availTopic, sizeof(availTopic), "status");
    doc["availability_topic"]    = availTopic;
    doc["availability_template"] = "{{ 'online' if value_json.online else 'offline' }}";

    char buf[512];
    size_t len = serializeJson(doc, buf, sizeof(buf));

    _mqtt.publish(discTopic, (const uint8_t*)buf, len, true);
}

// ── _topic ───────────────────────────────────────────────────
void MqttClient::_topic(char* buf, size_t bufLen, const char* subtopic) {
    char slug[32];
    _roomSlug(slug, sizeof(slug));
    snprintf(buf, bufLen, "%s/%s/%s",
             MQTT_TOPIC_ROOT, slug, subtopic);
}

// ── _roomSlug ────────────────────────────────────────────────
void MqttClient::_roomSlug(char* buf, size_t bufLen) {
    strncpy(buf, g_config.room_name, bufLen - 1);
    buf[bufLen - 1] = '\0';
    for (size_t i = 0; buf[i]; i++) {
        buf[i] = (char)tolower((unsigned char)buf[i]);
        if (buf[i] == ' ') buf[i] = '-';
    }
}

// ── MqttClient::publishWeather ───────────────────────────────
// Call from weather.cpp after successful fetch
void MqttClient::publishWeather(const WeatherData& w) {
    if (!_connected || !w.valid) return;
    char topic[80], payload[200];
    _topic(topic, sizeof(topic), "weather");
    snprintf(payload, sizeof(payload),
             "{\"temp_f\":%.1f,\"humidity\":%.0f,"
             "\"weather_code\":%u,\"uv_index\":%.1f}",
             w.temp_f, w.humidity, w.weather_code, w.uv_index);
    _mqtt.publish(topic, payload, true);
}
