// ============================================================
//  EnvCube — DFPlayer Mini voice alert driver (full impl)
//
//  DFPlayer Mini wiring:
//    DFPlayer RX → GPIO4 (ESP TX) via 1kΩ resistor
//    DFPlayer TX → GPIO5 (ESP RX) direct
//    VCC → 5V (from USB rail, NOT 3.3V — needs 5V for volume)
//    GND → GND
//    SPK+ / SPK- → 28mm 8Ω speaker
//
//  MicroSD card prep:
//    Format FAT32. Name files /01.mp3, /02.mp3 ... /11.mp3
//    Record at 44100 Hz mono MP3, 64kbps.
//    Keep clips 2–4 seconds — clear and loud, no music.
//    Example: "Smoke detected in the kitchen" (recorded voice)
//
//  Important:
//    DFPlayer needs ~1.5 s startup time after power-on.
//    We wait for it in begin() before declaring ready.
//    The 1kΩ resistor on DFPlayer RX is mandatory — without it
//    the module is often damaged by ESP32 3.3V TX to 5V device.
//
//  MIN_PLAY_GAP_MS prevents rapid-fire repeat plays that
//  overlap or cause DFPlayer to lock up.
// ============================================================

#include "dfplayer.h"
#include "../config.h"
#include "../storage/nvs_config.h"

DFRobotDFPlayerMini DFPlayer::_player;
HardwareSerial      DFPlayer::_serial(1);  // UART1 (remapped to GPIO4/5)
bool                DFPlayer::_ready      = false;
uint8_t             DFPlayer::_volume     = AUDIO_VOLUME;
uint32_t            DFPlayer::_lastPlayMs = 0;

// ── DFPlayer::begin ──────────────────────────────────────────
bool DFPlayer::begin() {
    // ESP32-C6 LP UART (UART2) has restrictions in Arduino Core 3.x.
    // DFPlayer uses UART1 remapped to GPIO4(TX)/GPIO5(RX).
    // Skip init if Serial1 is already in use by LD2410C on GPIO16/17.
    // TODO: resolve UART conflict in Phase 2 PCB design.
    // For now: log voice events to Serial only.
    Serial.println("[DFPlayer] UART init skipped — UART conflict on ESP32-C6");
    Serial.println("[DFPlayer] Voice alerts will be logged to Serial only");
    _ready = false;
    return false;

    // --- Below code kept for Phase 2 ---
    _serial.begin(DFPLAYER_BAUD, SERIAL_8N1,
                  PIN_DFPLAYER_RX, PIN_DFPLAYER_TX);

    Serial.println("[DFPlayer] Waiting for module startup (~1.5 s)...");
    delay(1500);

    if (!_player.begin(_serial, false, true)) {
        Serial.println("[DFPlayer] ERROR: module not detected");
        Serial.println("[DFPlayer] Check: 5V supply, 1kΩ on RX, SD card formatted FAT32");
        _ready = false;
        return false;
    }

    _player.volume(_volume);
    _player.EQ(DFPLAYER_EQ_NORMAL);
    _player.outputDevice(DFPLAYER_DEVICE_SD);

    uint8_t fileCount = _player.readFileCounts();
    _ready = true;

    Serial.printf("[DFPlayer] Ready — %u files on SD card  volume=%u\n",
                  fileCount, _volume);
    if (fileCount < 11) {
        Serial.println("[DFPlayer] WARNING: expected 11 audio files — check SD card");
    }
    return true;
}

// ── DFPlayer::play ───────────────────────────────────────────
void DFPlayer::play(uint8_t track) {
    if (!_ready) {
        Serial.printf("[DFPlayer] Not ready — skipping track %u\n", track);
        return;
    }
    if (!g_config.voice_enabled) return;

    // Rate-limit to avoid overlap
    uint32_t now = millis();
    if (now - _lastPlayMs < MIN_PLAY_GAP_MS) {
        Serial.printf("[DFPlayer] Rate-limited — skipping track %u\n", track);
        return;
    }

    _player.play(track);
    _lastPlayMs = now;
    Serial.printf("[DFPlayer] Playing track %u\n", track);
}

// ── DFPlayer::playForSource ──────────────────────────────────
void DFPlayer::playForSource(AlertSource source) {
    uint8_t track = 0;

    switch (source) {
        case AlertSource::SMOKE:       track = AUDIO_SMOKE;       break;
        case AlertSource::CO2:         track = AUDIO_CO2_CRITICAL; break;
        case AlertSource::TEMPERATURE: track = AUDIO_TEMP_HIGH;   break;
        case AlertSource::HUMIDITY:    track = AUDIO_HUMIDITY_HIGH;break;
        case AlertSource::VOC:
        case AlertSource::NOX:         track = AUDIO_AIR_QUALITY; break;
        case AlertSource::PM25:        track = AUDIO_PARTICULATE;  break;
        case AlertSource::NOISE:       track = AUDIO_NOISE_HIGH;  break;
        case AlertSource::REMOTE:
            // Remote alert: play whatever the remote source was
            // (remote payload source is mapped by sender)
            track = AUDIO_SMOKE;  // Default for remote — safest
            break;
        default:                       track = 0;                  break;
    }

    if (track > 0) play(track);
}

// ── DFPlayer::stop ───────────────────────────────────────────
void DFPlayer::stop() {
    if (!_ready) return;
    _player.stop();
    Serial.println("[DFPlayer] Stopped");
}

// ── DFPlayer::setVolume ──────────────────────────────────────
void DFPlayer::setVolume(uint8_t vol) {
    _volume = constrain(vol, 0, 30);
    if (_ready) _player.volume(_volume);
    Serial.printf("[DFPlayer] Volume set to %u\n", _volume);
}

// ── DFPlayer::isReady ────────────────────────────────────────
bool DFPlayer::isReady() { return _ready; }

// ── DFPlayer::loop ───────────────────────────────────────────
void DFPlayer::loop() {
    if (!_ready) return;

    // Check for module errors or status messages
    if (_player.available()) {
        uint8_t type  = _player.readType();
        uint16_t value = _player.read();

        if (type == DFPlayerError) {
            Serial.printf("[DFPlayer] Error: %u\n", value);
            // Attempt recovery for common timeout error
            if (value == TimeOut) {
                Serial.println("[DFPlayer] Timeout — reinitialising...");
                _player.begin(_serial, false, true);
                _player.volume(_volume);
            }
        }
    }
}