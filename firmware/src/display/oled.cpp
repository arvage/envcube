// ============================================================
//  EnvCube — OLED display driver (full implementation)
//
//  SSD1306 128×64 monochrome OLED via I²C (address 0x3C).
//  Uses Adafruit SSD1306 + Adafruit GFX libraries.
//
//  Layout constants:
//    Screen width:  128 px
//    Screen height:  64 px
//    Header row:     y=0  (8px tall)
//    Content area:   y=10 to y=54 (3 rows of content)
//    Footer row:     y=56 (8px tall)
//
//  Font: built-in 6×8 pixel font (GFX default)
//    Each char = 6px wide, 8px tall
//    Max chars per row = 128/6 = 21 chars
// ============================================================

#include "oled.h"
#include "../alerts/espnow_mesh.h"
#include "../config.h"
#include "../storage/nvs_config.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Global weather data — populated by weather.cpp
WeatherData g_weather = {};

static Adafruit_SSD1306 _display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

uint8_t  OledDisplay::_screen        = 0;
bool     OledDisplay::_dimmed        = false;
bool     OledDisplay::_otaActive     = false;
uint32_t OledDisplay::_lastActivityMs = 0;
bool     OledDisplay::_ready         = false;

// ── OledDisplay::begin ───────────────────────────────────────
void OledDisplay::begin() {
    if (!_display.begin(SSD1306_SWITCHCAPVCC, I2C_ADDR_OLED)) {
        Serial.println("[OLED] ERROR: display not found (0x3C)");
        _ready = false;
        return;
    }

    _display.clearDisplay();
    _display.setTextColor(SSD1306_WHITE);
    _display.setTextSize(1);
    _display.cp437(true);  // Use full 256-char CP437 font

    _ready          = true;
    _lastActivityMs = millis();
    Serial.println("[OLED] Initialised — 128×64 SSD1306");
}

// ── OledDisplay::update ──────────────────────────────────────
void OledDisplay::update(const SensorReadings& r,
                          AlertLevel level,
                          bool wifiConnected,
                          int rssi) {
    if (!_ready || _otaActive) return;

    // Auto-dim after inactivity
    uint32_t now = millis();
    if (!_dimmed && (now - _lastActivityMs > DIM_TIMEOUT_MS)) {
        _display.dim(true);
        _dimmed = true;
    }

    _display.clearDisplay();

    switch (_screen) {
        case 0: _drawScreen0(r, level);               break;
        case 1: _drawScreen1(r, level);               break;
        case 2: _drawScreen2(wifiConnected, rssi);    break;
    }

    _drawFooter(level, wifiConnected);
    _display.display();
}

// ── Screen 0 — Indoor sensors ────────────────────────────────
void OledDisplay::_drawScreen0(const SensorReadings& r, AlertLevel level) {
    // ── Header ───────────────────────────────────────────────
    _drawHeader(g_config.room_name, level);

    // ── Row 1: Temperature | Humidity | Pressure ─────────────
    _display.setCursor(0, 11);
    if (r.thermal_ok) {
        // Temperature with degree symbol (CP437 char 248 = °)
        _display.printf("T:%.1f\xF8 H:%.0f%%", r.temperature_c, r.humidity_rh);
    } else {
        _display.print("T:--- H:---");
    }
    // Pressure right-aligned
    if (r.thermal_ok) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%4.0fhP", r.pressure_hpa);
        _display.setCursor(128 - (strlen(buf) * 6), 11);
        _display.print(buf);
    }

    // ── Row 2: CO2 | VOC | NOx ───────────────────────────────
    _display.setCursor(0, 22);
    if (r.co2_ok) {
        _display.printf("CO2:%-4u", r.co2_ppm);
    } else {
        _display.print("CO2:----");
    }
    if (r.aq_ok) {
        _display.printf(" V:%-3d N:%-3d", r.voc_index, r.nox_index);
    } else {
        _display.print(" V:--- N:---");
    }

    // ── Row 3: PM2.5 | Noise | Lux ───────────────────────────
    _display.setCursor(0, 33);
    if (r.pm_ok) {
        _display.printf("PM:%-3u", r.pm2_5);
    } else {
        _display.print("PM:---");
    }
    if (r.noise_ok) {
        _display.printf(" Nz:%.0fdB", r.noise_db);
    } else {
        _display.print(" Nz:---");
    }
    if (r.lux > 0) {
        char lbuf[8];
        snprintf(lbuf, sizeof(lbuf), " %4ulx", (unsigned)r.lux);
        _display.setCursor(128 - (strlen(lbuf) * 6), 33);
        _display.print(lbuf);
    }

    // ── Row 4: Smoke status | Presence ───────────────────────
    _display.setCursor(0, 44);
    if (r.smoke_ok) {
        bool smoke = r.smoke_raw >= g_config.thresh_smoke_warn;
        _display.printf("Smoke:%-3s", smoke ? "YES" : "OK ");
    } else {
        _display.print("Smoke:---");
    }
    if (r.presence_ok) {
        _display.printf(" Pres:%-3s", r.presence ? "YES" : "NO ");
        if (r.presence && r.presence_cm > 0) {
            _display.printf(" %ucm", r.presence_cm);
        }
    }
}

// ── Screen 1 — Alert status ──────────────────────────────────
void OledDisplay::_drawScreen1(const SensorReadings& r, AlertLevel level) {
    // ── Header ───────────────────────────────────────────────
    _drawHeader("Alert Status", level);

    // ── Alert level banner ────────────────────────────────────
    _display.setCursor(0, 11);
    switch (level) {
        case AlertLevel::ALL_CLEAR:
            _display.print("  \x01 All systems normal");  // smiley
            break;
        case AlertLevel::WARNING:
            _display.print("  ! WARNING detected");
            break;
        case AlertLevel::ALERT:
            _display.print("  !! ALERT - check now");
            break;
        case AlertLevel::CRITICAL:
            _display.print("  !!! CRITICAL - act now");
            break;
    }

    // ── Sensor status grid ────────────────────────────────────
    // Left column
    _display.setCursor(0, 22);
    _display.printf("CO2 :%4uppm", r.co2_ok ? r.co2_ppm : 0);
    _display.setCursor(0, 31);
    _display.printf("Temp:%.1f\xF8%c ", r.thermal_ok ? r.temperature_c : 0.0f,
                    r.thermal_ok ? 'C' : '-');
    _display.setCursor(0, 40);
    _display.printf("PM25:%3ug/m3", r.pm_ok ? r.pm2_5 : 0);
    _display.setCursor(0, 49);
    _display.printf("VOC :%3d idx", r.aq_ok ? r.voc_index : 0);

    // Right column
    _display.setCursor(72, 22);
    bool smoke = r.smoke_ok && (r.smoke_raw >= g_config.thresh_smoke_warn);
    _display.printf("Smoke:%s", smoke ? "YES" : "OK ");
    _display.setCursor(72, 31);
    _display.printf("Hum :%2.0f%%", r.thermal_ok ? r.humidity_rh : 0.0f);
    _display.setCursor(72, 40);
    _display.printf("Nz :%.0fdB", r.noise_ok ? r.noise_db : 0.0f);
    _display.setCursor(72, 49);
    _display.printf("Pres:%s", r.presence_ok ? (r.presence ? "YES" : "NO ") : "---");
}

// ── Screen 2 — System info ───────────────────────────────────
void OledDisplay::_drawScreen2(bool wifiConnected, int rssi) {
    _drawHeader("System", AlertLevel::ALL_CLEAR);

    // ── Row 1: IP + RSSI ─────────────────────────────────────
    _display.setCursor(0, 11);
    if (wifiConnected) {
        // IP shown by wifi_manager, we just show RSSI here
        _display.printf("WiFi: %ddBm", rssi);
    } else {
        _display.print("WiFi: offline");
    }

    // ── Row 2: Room name + cube ID ────────────────────────────
    _display.setCursor(0, 22);
    _display.printf("Room: %s", g_config.room_name);
    _display.setCursor(96, 22);
    _display.printf("ID:%u", g_config.cube_id);

    // ── Row 3: Firmware version + uptime ─────────────────────
    _display.setCursor(0, 33);
    uint32_t upSec  = millis() / 1000;
    uint32_t upHr   = upSec / 3600;
    uint32_t upMin  = (upSec % 3600) / 60;
    _display.printf("v%s Up:%luh%02lum P:%u",
                    ENVCUBE_VERSION, upHr, upMin,
                    EspNowMesh::peerCount());

    // ── Row 4: Outdoor weather (if available) ─────────────────
    _display.setCursor(0, 44);
    if (g_weather.valid) {
        _display.printf("Out:%.0fF %.0f%% %s",
                        g_weather.temp_f,
                        g_weather.humidity,
                        _weatherDesc(g_weather.weather_code));
    } else {
        _display.print("Out: -- (no weather)");
    }
}

// ── showBoot ─────────────────────────────────────────────────
void OledDisplay::showBoot(const char* version) {
    if (!_ready) return;
    _display.clearDisplay();

    // Logo area
    _display.setTextSize(2);
    _display.setCursor(14, 8);
    _display.print("EnvCube");
    _display.setTextSize(1);

    // Version + tagline
    _display.setCursor(28, 30);
    _display.printf("v%s", version);
    _display.setCursor(4, 42);
    _display.print("Smart Environmental");
    _display.setCursor(16, 52);
    _display.print("Monitor");

    _display.display();
    _lastActivityMs = millis();
}

// ── showProvisioning ─────────────────────────────────────────
void OledDisplay::showProvisioning(const char* ssid) {
    if (!_ready) return;
    _display.clearDisplay();

    _display.setTextSize(1);
    _display.setCursor(10, 0);
    _display.print("[ SETUP MODE ]");

    _display.setCursor(0, 14);
    _display.print("1. Connect to WiFi:");
    _display.setCursor(4, 24);
    _display.print(ssid);

    _display.setCursor(0, 36);
    _display.print("2. Browser opens");
    _display.setCursor(0, 46);
    _display.print("3. Enter WiFi + room");

    _display.display();
    _lastActivityMs = millis();
}

// ── showOtaProgress ──────────────────────────────────────────
void OledDisplay::showOtaProgress(uint8_t percent) {
    if (!_ready) return;
    _otaActive = true;
    _display.clearDisplay();

    _display.setTextSize(1);
    _display.setCursor(16, 8);
    _display.print("Updating firmware");

    // Progress bar
    _display.drawRect(4, 28, 120, 12, SSD1306_WHITE);
    uint8_t filled = (uint8_t)(116 * percent / 100);
    _display.fillRect(6, 30, filled, 8, SSD1306_WHITE);

    _display.setCursor(54, 44);
    _display.printf("%u%%", percent);

    _display.display();
}

// ── nextScreen ───────────────────────────────────────────────
void OledDisplay::nextScreen() {
    _screen = (_screen + 1) % SCREEN_COUNT;
    _lastActivityMs = millis();
    if (_dimmed) {
        _display.dim(false);
        _dimmed = false;
    }
    Serial.printf("[OLED] Screen -> %u\n", _screen);
}

// ── setScreen ────────────────────────────────────────────────
void OledDisplay::setScreen(uint8_t screen) {
    if (screen < SCREEN_COUNT) {
        _screen = screen;
        _lastActivityMs = millis();
    }
}

// ── setDim ───────────────────────────────────────────────────
void OledDisplay::setDim(bool dim) {
    _display.dim(dim);
    _dimmed = dim;
    if (!dim) _lastActivityMs = millis();
}

uint8_t OledDisplay::currentScreen() { return _screen; }

// ── _drawHeader ──────────────────────────────────────────────
void OledDisplay::_drawHeader(const char* title, AlertLevel level) {
    // Filled header bar
    _display.fillRect(0, 0, OLED_WIDTH, 9, SSD1306_WHITE);
    _display.setTextColor(SSD1306_BLACK);
    _display.setCursor(2, 1);
    _display.print(title);

    // Alert indicator dot top-right
    uint8_t dotX = OLED_WIDTH - 8;
    switch (level) {
        case AlertLevel::ALL_CLEAR:
            _display.drawCircle(dotX, 4, 3, SSD1306_BLACK);
            break;
        case AlertLevel::WARNING:
            _display.fillCircle(dotX, 4, 3, SSD1306_BLACK);
            break;
        case AlertLevel::ALERT:
        case AlertLevel::CRITICAL:
            // Filled square for critical
            _display.fillRect(dotX-3, 1, 7, 7, SSD1306_BLACK);
            break;
    }

    _display.setTextColor(SSD1306_WHITE);
}

// ── _drawFooter ──────────────────────────────────────────────
void OledDisplay::_drawFooter(AlertLevel level, bool wifi) {
    // Thin separator line
    _display.drawFastHLine(0, 55, OLED_WIDTH, SSD1306_WHITE);

    _display.setCursor(0, 57);
    _display.setTextSize(1);

    // Alert label left
    _display.print(_alertLabel(level));

    // WiFi icon right (simple W or X)
    _display.setCursor(116, 57);
    _display.print(wifi ? "W" : "-");
}

// ── _alertLabel ──────────────────────────────────────────────
const char* OledDisplay::_alertLabel(AlertLevel level) {
    switch (level) {
        case AlertLevel::ALL_CLEAR: return "OK";
        case AlertLevel::WARNING:   return "WARN";
        case AlertLevel::ALERT:     return "ALERT";
        case AlertLevel::CRITICAL:  return "!!CRIT";
        default:                    return "---";
    }
}

// ── _weatherDesc ─────────────────────────────────────────────
// WMO weather code → short description
const char* OledDisplay::_weatherDesc(uint8_t code) {
    if (code == 0)              return "Clear";
    if (code <= 2)              return "PrtCld";
    if (code == 3)              return "Ovrcst";
    if (code <= 49)             return "Fog";
    if (code <= 59)             return "Drzl";
    if (code <= 69)             return "Rain";
    if (code <= 79)             return "Snow";
    if (code <= 82)             return "Shwrs";
    if (code <= 99)             return "Storm";
    return "---";
}
