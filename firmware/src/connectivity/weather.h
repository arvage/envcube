#pragma once
// ============================================================
//  EnvCube — Open-Meteo weather fetch
//  Free API · no key · REST over HTTPS
//
//  Endpoint:
//  https://api.open-meteo.com/v1/forecast
//    ?latitude=<lat>
//    &longitude=<lon>
//    &current=temperature_2m,relative_humidity_2m,
//             weather_code,uv_index,apparent_temperature
//    &temperature_unit=fahrenheit
//    &wind_speed_unit=mph
//
//  Updates every WEATHER_UPDATE_MS (10 min default).
//  Populates g_weather struct read by OLED Screen 2.
//  Gracefully skips if WiFi not connected.
// ============================================================

#include <Arduino.h>
#include "../display/oled.h"   // WeatherData + g_weather

class Weather {
public:
    static void loop();
    static void fetchNow();
    static void requestFetch();   // safe to call from any context — picked up by loop()
    static bool isValid();

private:
    static bool     _fetch();
    static uint32_t _lastFetchMs;
    static bool     _valid;
    static volatile bool _fetchRequested;
public:
    static char     _lastError[64];
};
