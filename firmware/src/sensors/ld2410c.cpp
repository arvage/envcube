// ============================================================
//  EnvCube — HLK-LD2410C presence sensor UART driver
//
//  Protocol:
//  LD2410C outputs data frames at ~10Hz over UART 256000 baud.
//  Frame format (basic reporting mode, 13 bytes):
//
//  [F4 F3 F2 F1] [0D 00] [02] [00] [target_state]
//  [move_dist_L] [move_dist_H] [move_energy]
//  [still_dist_L] [still_dist_H] [still_energy]
//  [detect_dist_L] [detect_dist_H]
//  [F8 F7 F6 F5]
//
//  target_state:
//    0x00 = no target
//    0x01 = moving target
//    0x02 = stationary target
//    0x03 = both moving + stationary
//
//  Notes:
//  - UART2 used on ESP32-C6 (GPIO16 TX, GPIO17 RX)
//  - No pull-ups needed on UART — driven signals
//  - Module requires 3.3V supply (5V tolerant on signal pins)
//  - Pod face must be plain plastic (no metal) — radar must
//    penetrate through enclosure face
//  - Detection range: 0–500 cm (default config)
// ============================================================

#include "ld2410c.h"
#include "../config.h"

// Use HardwareSerial for ESP32-C6 UART
static HardwareSerial _ld2410Serial(1);  // UART1

bool     Ld2410c::_ready      = false;
bool     Ld2410c::_presence   = false;
uint16_t Ld2410c::_distanceCm = 0;

// ── Ld2410c::begin ───────────────────────────────────────────
bool Ld2410c::begin() {
    _ld2410Serial.begin(LD2410_BAUD, SERIAL_8N1, PIN_LD2410_RX, PIN_LD2410_TX);
    delay(100);  // Allow UART to settle

    // Flush any startup noise from module
    while (_ld2410Serial.available()) _ld2410Serial.read();

    _ready = true;
    Serial.println("[LD2410C] Initialised — UART 256000 baud");
    Serial.printf("[LD2410C] TX→GPIO%d  RX←GPIO%d\n",
                  PIN_LD2410_TX, PIN_LD2410_RX);
    return true;
}

// ── Ld2410c::read ────────────────────────────────────────────
bool Ld2410c::read(SensorReadings& r) {
    if (!_ready) {
        r.presence_ok = false;
        return false;
    }

    // Drain incoming bytes and look for valid frames
    static uint8_t  buf[32];
    static uint8_t  bufIdx = 0;
    static bool     inFrame = false;
    bool            gotReading = false;

    while (_ld2410Serial.available()) {
        uint8_t b = _ld2410Serial.read();

        if (!inFrame) {
            // Look for frame start sequence F4 F3 F2 F1
            if (bufIdx == 0 && b == FRAME_HEADER_1) { buf[bufIdx++] = b; }
            else if (bufIdx == 1 && b == FRAME_HEADER_2) { buf[bufIdx++] = b; }
            else if (bufIdx == 2 && b == FRAME_HEADER_3) { buf[bufIdx++] = b; }
            else if (bufIdx == 3 && b == FRAME_HEADER_4) { buf[bufIdx++] = b; inFrame = true; }
            else { bufIdx = 0; }  // Reset on mismatch
        } else {
            // Inside frame — accumulate bytes
            if (bufIdx < sizeof(buf)) {
                buf[bufIdx++] = b;
            } else {
                // Buffer overflow — reset
                bufIdx  = 0;
                inFrame = false;
                continue;
            }

            // Check for end sequence F8 F7 F6 F5
            if (bufIdx >= 8 &&
                buf[bufIdx-4] == FRAME_END_1 &&
                buf[bufIdx-3] == FRAME_END_2 &&
                buf[bufIdx-2] == FRAME_END_3 &&
                buf[bufIdx-1] == FRAME_END_4) {

                if (_parseFrame(buf, bufIdx)) {
                    gotReading = true;
                }
                bufIdx  = 0;
                inFrame = false;
            }
        }
    }

    if (gotReading || r.presence_ok) {
        r.presence    = _presence;
        r.presence_cm = _distanceCm;
        r.presence_ok = true;

        Serial.printf("[LD2410C] %s  dist=%ucm\n",
                      _presence ? "PRESENT" : "empty",
                      _distanceCm);
    }

    return true;
}

// ── _parseFrame ──────────────────────────────────────────────
bool Ld2410c::_parseFrame(const uint8_t* buf, uint8_t len) {
    // Minimum valid basic report frame is ~23 bytes
    if (len < 23) return false;

    // Byte offset after 4-byte header + 2-byte length + 2 type bytes:
    // buf[8] = target state
    // buf[9..10] = moving target distance (little-endian)
    // buf[12..13] = stationary target distance
    // buf[15..16] = detected target distance

    uint8_t  targetState    = buf[8];
    uint16_t detectedDist   = (uint16_t)buf[15] | ((uint16_t)buf[16] << 8);

    _presence   = (targetState != 0x00);
    _distanceCm = _presence ? detectedDist : 0;

    return true;
}

bool     Ld2410c::isReady()          { return _ready; }
bool     Ld2410c::presenceDetected() { return _presence; }
uint16_t Ld2410c::targetDistanceCm() { return _distanceCm; }
