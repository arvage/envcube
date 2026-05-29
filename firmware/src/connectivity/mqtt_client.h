#pragma once
// ============================================================
//  EnvCube — MQTT client + Home Assistant auto-discovery
//  PubSubClient library · broker set during provisioning
//
//  Topic structure:
//    envcube/<room>/temperature   {"value":22.3,"unit":"C","ok":true}
//    envcube/<room>/humidity      {"value":48.1,"unit":"%","ok":true}
//    envcube/<room>/co2           {"value":612,"unit":"ppm","ok":true}
//    envcube/<room>/smoke         {"value":false,"raw":320,"ok":true}
//    envcube/<room>/voc_index     {"value":45,"ok":true}
//    envcube/<room>/nox_index     {"value":4,"ok":true}
//    envcube/<room>/pm25          {"value":8,"unit":"ug/m3","ok":true}
//    envcube/<room>/noise_db      {"value":42.1,"unit":"dBA","ok":true}
//    envcube/<room>/lux           {"value":340,"unit":"lx","ok":true}
//    envcube/<room>/pressure      {"value":1013.2,"unit":"hPa","ok":true}
//    envcube/<room>/presence      {"value":true,"distance_cm":180,"ok":true}
//    envcube/<room>/alert         {"level":0,"source":"none","message":"All clear"}
//    envcube/<room>/status        {"online":true,"rssi":-68,"ip":"...","uptime":3600}
//    envcube/<room>/heartbeat     {"ts":1735000000,"cube_id":42}
//
//  HA auto-discovery published to:
//    homeassistant/sensor/envcube_<room>_<sensor>/config
//    homeassistant/binary_sensor/envcube_<room>_<sensor>/config
// ============================================================

#include <Arduino.h>
#include "../alert_types.h"
#include "../display/oled.h"   // WeatherData

class MqttClient {
public:
    // Initialise — call after WiFi is connected
    static void begin();

    // Call from connectivity task loop
    static void loop();

    // Publish all sensor readings
    static void publishReadings(const SensorReadings& r);

    // Publish alert state
    static void publishAlert(AlertLevel level, AlertSource source,
                             const char* message);

    // Publish heartbeat (watchdog ping)
    static void publishHeartbeat();

    // Publish online status
    static void publishStatus(bool online, int rssi, const char* ip,
                              uint32_t uptime_s);

    // True if currently connected to broker
    static bool isConnected();

private:
    static void _connect();
    static void _publishDiscovery();
    static void _publishSensor(const char* sensor_id,
                               const char* name,
                               const char* unit,
                               const char* device_class,
                               const char* state_topic,
                               const char* value_template,
                               bool binary = false);
    static void _onMessage(char* topic, byte* payload, unsigned int length);

    // Build topic: envcube/<room>/<subtopic>
    static void _topic(char* buf, size_t bufLen, const char* subtopic);
    // Build slugified room name (lowercase, spaces→hyphens)
    static void _roomSlug(char* buf, size_t bufLen);

    static bool     _connected;
    static bool     _discoveryPublished;
    static uint32_t _lastReconnectMs;
    static uint32_t _lastPublishMs;

    static const uint32_t RECONNECT_INTERVAL_MS = 5000;
    static const uint32_t PUBLISH_INTERVAL_MS   = 10000;
};

    // Publish outdoor weather (called from weather.cpp after fetch)
    static void publishWeather(const WeatherData& w);
