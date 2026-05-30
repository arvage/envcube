// ============================================================
//  EnvCube — In-memory log ring buffer
//  Explicit push model: call Logger::write() from anywhere.
//  Exposed via GET /api/log as {seq, lines[]}.
// ============================================================

#include "logger.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"

static const int LOG_LINES = 80;
static const int LINE_LEN  = 128;

static char _buf[LOG_LINES][LINE_LEN];
static int  _head  = 0;
static int  _count = 0;

static void _push(const char* line) {
    int len = strlen(line);
    // strip trailing newline
    char tmp[LINE_LEN];
    strlcpy(tmp, line, LINE_LEN);
    while (len > 0 && (tmp[len-1] == '\n' || tmp[len-1] == '\r')) tmp[--len] = '\0';
    if (len == 0) return;
    strlcpy(_buf[_head], tmp, LINE_LEN);
    _head  = (_head + 1) % LOG_LINES;
    _count++;
}

// IDF log vprintf hook — also write to ring buffer
static vprintf_like_t _origVprintf = nullptr;
static int _logHook(const char* fmt, va_list args) {
    int n = _origVprintf ? _origVprintf(fmt, args) : 0;
    char tmp[LINE_LEN];
    va_list a2; va_copy(a2, args);
    vsnprintf(tmp, sizeof(tmp), fmt, a2);
    va_end(a2);
    _push(tmp);
    return n;
}

void Logger::begin() {
    _origVprintf = esp_log_set_vprintf(_logHook);
    Logger::write('I', "Log", "Logger started");
}

void Logger::write(char level, const char* tag, const char* fmt, ...) {
    char msg[LINE_LEN - 32];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    // Timestamp in seconds from boot
    uint32_t ts = (uint32_t)(esp_timer_get_time() / 1000000ULL);

    char line[LINE_LEN];
    snprintf(line, sizeof(line), "[%4us][%c][%s] %s", ts, level, tag, msg);
    _push(line);
}

void Logger::getJson(char* buf, size_t bufLen) {
    size_t pos = 0;

    auto w = [&](const char* s) {
        size_t l = strlen(s);
        if (pos + l + 1 < bufLen) { memcpy(buf+pos, s, l); pos += l; }
    };

    char hdr[40];
    snprintf(hdr, sizeof(hdr), "{\"seq\":%d,\"lines\":[", _count);
    w(hdr);

    int total = (_count < LOG_LINES) ? _count : LOG_LINES;
    int start = (_head - total + LOG_LINES) % LOG_LINES;

    for (int i = 0; i < total; i++) {
        if (i > 0) w(",");
        w("\"");
        const char* src = _buf[(start + i) % LOG_LINES];
        while (*src && pos + 6 < bufLen) {
            unsigned char c = (unsigned char)*src++;
            if      (c == '"')  { buf[pos++]='\\'; buf[pos++]='"'; }
            else if (c == '\\') { buf[pos++]='\\'; buf[pos++]='\\'; }
            else if (c == '\n') { buf[pos++]='\\'; buf[pos++]='n'; }
            else if (c == '\r') { /* skip */ }
            else if (c < 0x20)  { /* skip control chars */ }
            else                { buf[pos++]=c; }
        }
        w("\"");
    }
    w("]}");
    buf[pos] = '\0';
}
