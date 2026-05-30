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
#include "../web/logger.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#ifdef ENVCUBE_ENABLE_MQTT
#include "mqtt_client.h"
#endif

uint32_t         Weather::_lastFetchMs    = 0;
bool             Weather::_valid          = false;
volatile bool    Weather::_fetchRequested = false;
char             Weather::_lastError[64]  = "never fetched";

static const char* HOST = "api.open-meteo.com";
static const int   PORT = 80;

// ── Weather::loop ────────────────────────────────────────────
void Weather::loop() {
    if (g_config.latitude == 0.0f && g_config.longitude == 0.0f) return;
    bool timerExpired = (millis() - _lastFetchMs >= WEATHER_UPDATE_MS);
    if (!_fetchRequested && !timerExpired) return;
    _fetchRequested = false;
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

// ── Weather::requestFetch — safe from any task/callback ──────
void Weather::requestFetch() {
    _fetchRequested = true;
}

bool Weather::isValid() { return _valid; }

// ── Weather::_fetch ──────────────────────────────────────────
bool Weather::_fetch() {
    strlcpy(_lastError, "connecting...", sizeof(_lastError));

    char url[256];
    snprintf(url, sizeof(url),
        "http://" "%s" "/v1/forecast"
        "?latitude=%.4f&longitude=%.4f"
        "&current=temperature_2m,relative_humidity_2m,"
        "weather_code,uv_index,apparent_temperature"
        "&temperature_unit=fahrenheit&wind_speed_unit=mph&timezone=auto",
        HOST, g_config.latitude, g_config.longitude);

    HTTPClient http;
    http.begin(url);
    http.setTimeout(10000);
    http.setUserAgent("EnvCube/" ENVCUBE_VERSION);

    int code = http.GET();
    if (code != 200) {
        snprintf(_lastError, sizeof(_lastError), "HTTP %d", code);
        http.end();
        return false;
    }
    strlcpy(_lastError, "parsing response...", sizeof(_lastError));

    String body = http.getString();
    http.end();

    if (body.length() == 0) {
        strlcpy(_lastError, "empty response", sizeof(_lastError));
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        snprintf(_lastError, sizeof(_lastError), "JSON: %s", err.c_str());
        return false;
    }

    JsonObject current = doc["current"];
    if (current.isNull()) {
        snprintf(_lastError, sizeof(_lastError), "no current: %.40s", body.c_str());
        return false;
    }

    // Populate global weather struct (read by OLED Screen 2)
    g_weather.temp_f       = current["temperature_2m"]        | 0.0f;
    g_weather.humidity     = current["relative_humidity_2m"]  | 0.0f;
    g_weather.weather_code = current["weather_code"]           | 0;
    g_weather.uv_index     = current["uv_index"]               | 0.0f;
    g_weather.valid        = true;
    g_weather.fetched_ms   = millis();
    _lastFetchMs           = millis();
    _valid                 = true;
    snprintf(_lastError, sizeof(_lastError), "ok: %.1fF %.0f%%RH",
             g_weather.temp_f, g_weather.humidity);
    Logger::write('I', "Weather", "%.1f°F  %.0f%%RH  UV:%.1f  code:%u",
                g_weather.temp_f, g_weather.humidity,
                g_weather.uv_index, g_weather.weather_code);

#ifdef ENVCUBE_ENABLE_MQTT
    MqttClient::publishWeather(g_weather);
#endif
    return true;
}
