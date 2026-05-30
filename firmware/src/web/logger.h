#pragma once
#include <Arduino.h>

// Ring-buffer logger — captures output via IDF vprintf hook and explicit
// Logger::write() calls. Exposed as JSON via GET /api/log.
namespace Logger {
    void begin();
    void write(char level, const char* tag, const char* fmt, ...);
    void getJson(char* buf, size_t bufLen);
}
