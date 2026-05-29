// ============================================================
//  EnvCube — Open-Meteo weather fetch implementation
//
//  Uses WiFiClientSecure for HTTPS.
//  ArduinoJson parses the response.
//
//  Open-Meteo current weather response (relevant fields):
//  {
//    "current": {
//      "temperature_2m": 72.4,
//      "relative_humidity_2m": 58,
//      "apparent_temperature": 70.1,
//      "weather_code": 3,
//      "uv_index": 4.5
//    }
//  }
//
//  WMO weather codes (weather_code):
//    0        = Clear sky
//    1,2,3    = Mainly clear, partly cloudy, overcast
//    45,48    = Fog
//    51-57    = Drizzle
//    61-67    = Rain
//    71-77    = Snow
//    80-82    = Rain showers
//    95-99    = Thunderstorm
// ============================================================

#include "weather.h"
#include "../config.h"
#include "../storage/nvs_config.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#ifdef ENVCUBE_ENABLE_MQTT
#include "mqtt_client.h"
#endif

uint32_t Weather::_lastFetchMs = 0;
bool     Weather::_valid       = false;

static const char* HOST = "api.open-meteo.com";
static const int   PORT = 443;

// Open-Meteo root CA (ISRG Root X1 — Let's Encrypt)
// Valid until 2035. Update if HTTPS fails after expiry.
static const char* ROOT_CA = \
"-----BEGIN CERTIFICATE-----\n"
"MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n"
"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"
"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n"
"WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu\n"
"ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n"
"MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoBggIBAK3oJHP0FDfzm54rVygc\n"
"h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+\n"
"0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U\n"
"A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW\n"
"T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH\n"
"B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC\n"
"B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv\n"
"KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn\n"
"OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn\n"
"jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw\n"
"qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI\n"
"rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV\n"
"HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq\n"
"hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL\n"
"ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ\n"
"3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK\n"
"NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5\n"
"ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur\n"
"TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC\n"
"jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc\n"
"oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq\n"
"4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA\n"
"mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d\n"
"emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=\n"
"-----END CERTIFICATE-----\n";

// ── Weather::loop ────────────────────────────────────────────
void Weather::loop() {
    if (g_config.latitude == 0.0f && g_config.longitude == 0.0f) return;
    if (millis() - _lastFetchMs < WEATHER_UPDATE_MS) return;
    _fetch();
}

// ── Weather::fetchNow ────────────────────────────────────────
void Weather::fetchNow() {
    if (g_config.latitude == 0.0f && g_config.longitude == 0.0f) {
        Serial.println("[Weather] No location configured — skipping");
        return;
    }
    _fetch();
}

bool Weather::isValid() { return _valid; }

// ── Weather::_fetch ──────────────────────────────────────────
bool Weather::_fetch() {
    _lastFetchMs = millis();

    WiFiClientSecure client;
    client.setCACert(ROOT_CA);
    client.setTimeout(10);

    if (!client.connect(HOST, PORT)) {
        Serial.println("[Weather] ERROR: HTTPS connect failed");
        return false;
    }

    // Build request URL
    char path[256];
    snprintf(path, sizeof(path),
        "/v1/forecast"
        "?latitude=%.4f"
        "&longitude=%.4f"
        "&current=temperature_2m,relative_humidity_2m,"
        "weather_code,uv_index,apparent_temperature"
        "&temperature_unit=fahrenheit"
        "&wind_speed_unit=mph"
        "&timezone=auto",
        g_config.latitude, g_config.longitude);

    // Send HTTP GET
    client.printf("GET %s HTTP/1.1\r\n"
                  "Host: %s\r\n"
                  "Connection: close\r\n"
                  "User-Agent: EnvCube/%s\r\n"
                  "\r\n",
                  path, HOST, ENVCUBE_VERSION);

    // Wait for response
    unsigned long timeout = millis();
    while (!client.available()) {
        if (millis() - timeout > 10000) {
            Serial.println("[Weather] ERROR: response timeout");
            client.stop();
            return false;
        }
        delay(10);
    }

    // Skip HTTP headers
    while (client.available()) {
        String line = client.readStringUntil('\n');
        if (line == "\r") break;  // blank line = end of headers
    }

    // Read JSON body (max 2KB)
    String body = "";
    while (client.available()) {
        body += (char)client.read();
        if (body.length() > 2048) break;
    }
    client.stop();

    if (body.length() == 0) {
        Serial.println("[Weather] ERROR: empty response body");
        return false;
    }

    // Parse JSON
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        Serial.printf("[Weather] JSON parse error: %s\n", err.c_str());
        return false;
    }

    JsonObject current = doc["current"];
    if (current.isNull()) {
        Serial.println("[Weather] ERROR: no 'current' object in response");
        return false;
    }

    // Populate global weather struct (read by OLED Screen 2)
    g_weather.temp_f       = current["temperature_2m"]        | 0.0f;
    g_weather.humidity     = current["relative_humidity_2m"]  | 0.0f;
    g_weather.weather_code = current["weather_code"]           | 0;
    g_weather.uv_index     = current["uv_index"]               | 0.0f;
    g_weather.valid        = true;
    g_weather.fetched_ms   = millis();

    _valid = true;

    Serial.printf("[Weather] %.1f°F  %.0f%%RH  code:%u  UV:%.1f\n",
                  g_weather.temp_f, g_weather.humidity,
                  g_weather.weather_code, g_weather.uv_index);
#ifdef ENVCUBE_ENABLE_MQTT
    MqttClient::publishWeather(g_weather);
#endif
    return true;
}
