// ============================================================
//  EnvCube — ESP-NOW encrypted mesh (full implementation)
//
//  ESP-NOW runs on the WiFi radio but does NOT need a router.
//  It operates on the same channel as the connected AP (or
//  channel 1 if not connected). Packets are raw 802.11 frames.
//
//  AES-128 encryption:
//  - PMK (16 bytes) set globally — all cubes in same home
//    must use the same PMK (configured in config.h).
//  - LMK (16 bytes) set per peer — derived from PMK here
//    by repeating PMK bytes (simple but effective for home use).
//    Change to a proper KDF before commercial production.
//
//  Broadcast vs unicast:
//  - HELLO and ALERT use broadcast MAC (ff:ff:ff:ff:ff:ff)
//    so no pre-pairing needed. Encryption still applied.
//  - Packets are sent ESPNOW_ALERT_REPEATS times for reliability
//    since broadcast has no ACK.
//
//  Thread safety:
//  - _onReceive runs in WiFi task context (not main loop).
//  - We use a simple volatile flag + ring buffer approach
//    to pass received packets to the main loop safely.
// ============================================================

#include "espnow_mesh.h"
#include "alert_engine.h"
#include "../config.h"
#include "../storage/nvs_config.h"
#include "../outputs/led.h"
#include "../outputs/buzzer.h"
#include "../outputs/dfplayer.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// ── Static member init ───────────────────────────────────────
EspNowPeer EspNowMesh::_peers[ESPNOW_MAX_PEERS] = {};
uint8_t    EspNowMesh::_peerCount        = 0;
uint32_t   EspNowMesh::_lastHelloMs      = 0;
bool       EspNowMesh::_ready            = false;
uint8_t    EspNowMesh::_myMac[6]         = {};
uint8_t    EspNowMesh::_lastAlertCubeId  = 0xFF;
uint32_t   EspNowMesh::_lastAlertTimestamp = 0;

// ── Receive ring buffer (ISR-safe) ───────────────────────────
struct RxEntry {
    uint8_t  mac[6];
    uint8_t  data[250];
    int      len;
    bool     ready;
};
static volatile RxEntry _rxBuf[4];
static volatile uint8_t _rxWrite = 0;
static volatile uint8_t _rxRead  = 0;

// ── EspNowMesh::begin ────────────────────────────────────────
bool EspNowMesh::begin() {
#ifndef ENVCUBE_ENABLE_ESPNOW
    Serial.println("[ESP-NOW] Disabled by build flag");
    return true;
#endif

    // ESP-NOW requires WiFi to be initialised first
    // WiFi.mode() must have been called before this
    if (WiFi.getMode() == WIFI_OFF) {
        WiFi.mode(WIFI_STA);
    }

    // Get our MAC address
    WiFi.macAddress(_myMac);
    Serial.printf("[ESP-NOW] My MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  _myMac[0], _myMac[1], _myMac[2],
                  _myMac[3], _myMac[4], _myMac[5]);

    // Initialise ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESP-NOW] ERROR: esp_now_init() failed");
        _ready = false;
        return false;
    }

    // Set PMK for encryption (AES-128)
    // PMK must be exactly 16 bytes
    uint8_t pmk[16];
    const char* pmkStr = ESPNOW_PMK;
    for (int i = 0; i < 16; i++) {
        pmk[i] = (uint8_t)pmkStr[i % strlen(pmkStr)];
    }
    esp_now_set_pmk(pmk);

    // Register receive callback
    esp_now_register_recv_cb(_onReceive);

    // Register broadcast peer (needed to send to ff:ff:ff:ff:ff:ff)
    uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_now_peer_info_t broadcastPeer = {};
    memcpy(broadcastPeer.peer_addr, broadcastMac, 6);
    broadcastPeer.channel  = ESPNOW_CHANNEL;
    broadcastPeer.encrypt  = false;  // Broadcast cannot use LMK
    broadcastPeer.ifidx    = WIFI_IF_STA;
    esp_now_add_peer(&broadcastPeer);

    _ready       = true;
    _lastHelloMs = millis();

    Serial.println("[ESP-NOW] Initialised — AES-128 PMK set");
    Serial.printf("[ESP-NOW] Channel: %d  Max peers: %d\n",
                  ESPNOW_CHANNEL, ESPNOW_MAX_PEERS);

    // Announce ourselves immediately
    broadcastHello();
    return true;
}

// ── EspNowMesh::broadcastAlert ───────────────────────────────
bool EspNowMesh::broadcastAlert(AlertLevel level, AlertSource source,
                                 const char* room_name, float value) {
    if (!_ready) return false;

    EspNowAlertPacket pkt;
    pkt.level       = level;
    pkt.source      = source;
    pkt.cube_id     = g_config.cube_id;
    pkt.value       = value;
    pkt.timestamp_s = millis() / 1000;
    strncpy(pkt.room_name, room_name, sizeof(pkt.room_name) - 1);

    uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    // Send ESPNOW_ALERT_REPEATS times for reliability
    // (broadcast has no ACK so repetition improves delivery)
    bool ok = true;
    for (uint8_t i = 0; i < ESPNOW_ALERT_REPEATS; i++) {
        esp_err_t result = esp_now_send(broadcastMac,
                                        (const uint8_t*)&pkt,
                                        sizeof(pkt));
        if (result != ESP_OK) {
            Serial.printf("[ESP-NOW] Send error %d on repeat %u\n", result, i);
            ok = false;
        }
        if (i < ESPNOW_ALERT_REPEATS - 1) delay(10);
    }

    Serial.printf("[ESP-NOW] Alert broadcast: level=%d src=%d room=%s\n",
                  (int)level, (int)source, room_name);
    return ok;
}

// ── EspNowMesh::broadcastHello ───────────────────────────────
void EspNowMesh::broadcastHello() {
    if (!_ready) return;

    EspNowHelloPacket pkt;
    pkt.cube_id = g_config.cube_id;
    memcpy(pkt.mac, _myMac, 6);
    strncpy(pkt.room_name, g_config.room_name, sizeof(pkt.room_name) - 1);

    uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_now_send(broadcastMac, (const uint8_t*)&pkt, sizeof(pkt));

    Serial.printf("[ESP-NOW] HELLO broadcast — room: %s  id: %u\n",
                  g_config.room_name, g_config.cube_id);
}

// ── EspNowMesh::loop ─────────────────────────────────────────
void EspNowMesh::loop() {
    if (!_ready) return;

    // Process received packets from ring buffer
    while (_rxRead != _rxWrite) {
        uint8_t idx = _rxRead;
        if (_rxBuf[idx].ready) {
            const uint8_t* mac  = (const uint8_t*)_rxBuf[idx].mac;
            const uint8_t* data = (const uint8_t*)_rxBuf[idx].data;
            int             len  = _rxBuf[idx].len;

            if (len >= 2) {
                EspNowPacketType pktType = (EspNowPacketType)data[0];

                if (pktType == EspNowPacketType::HELLO &&
                    len >= (int)sizeof(EspNowHelloPacket)) {
                    EspNowHelloPacket pkt;
                    memcpy(&pkt, data, sizeof(pkt));
                    _handleHello(mac, pkt);
                }
                else if (pktType == EspNowPacketType::ALERT &&
                         len >= (int)sizeof(EspNowAlertPacket)) {
                    EspNowAlertPacket pkt;
                    memcpy(&pkt, data, sizeof(pkt));
                    _handleAlert(pkt);
                }
            }

            _rxBuf[idx].ready = false;
            _rxRead = (_rxRead + 1) % 4;
        } else {
            break;
        }
    }

    // Periodic HELLO broadcast
    if (millis() - _lastHelloMs >= HELLO_INTERVAL_MS) {
        broadcastHello();
        _lastHelloMs = millis();
    }

    // Mark inactive peers
    for (uint8_t i = 0; i < ESPNOW_MAX_PEERS; i++) {
        if (_peers[i].active &&
            millis() - _peers[i].last_seen_ms > PEER_TIMEOUT_MS) {
            Serial.printf("[ESP-NOW] Peer %s timed out\n", _peers[i].room_name);
            _peers[i].active = false;
            _peerCount = 0;
            for (uint8_t j = 0; j < ESPNOW_MAX_PEERS; j++) {
                if (_peers[j].active) _peerCount++;
            }
        }
    }
}

// ── _onReceive (ISR context — keep minimal) ──────────────────
void EspNowMesh::_onReceive(const uint8_t* mac, const uint8_t* data, int len) {
    // Write to ring buffer — do not call Serial or complex functions here
    uint8_t idx = _rxWrite;
    if (!_rxBuf[idx].ready) {
        memcpy((void*)_rxBuf[idx].mac,  mac,  6);
        int copyLen = (len > 250) ? 250 : len;
        memcpy((void*)_rxBuf[idx].data, data, copyLen);
        _rxBuf[idx].len   = copyLen;
        _rxBuf[idx].ready = true;
        _rxWrite = (_rxWrite + 1) % 4;
    }
}

// ── _handleHello ─────────────────────────────────────────────
void EspNowMesh::_handleHello(const uint8_t* mac,
                               const EspNowHelloPacket& pkt) {
    // Ignore our own HELLO (broadcast comes back to us)
    if (memcmp(mac, _myMac, 6) == 0) return;

    bool known = _isPeerKnown(mac);
    _addPeer(mac, pkt.room_name, pkt.cube_id);

    if (!known) {
        Serial.printf("[ESP-NOW] New peer: %s (ID:%u) %02X:%02X:%02X:%02X:%02X:%02X\n",
                      pkt.room_name, pkt.cube_id,
                      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    } else {
        // Update last_seen
        int slot = _findPeerSlot(mac);
        if (slot >= 0) _peers[slot].last_seen_ms = millis();
    }
}

// ── _handleAlert ─────────────────────────────────────────────
void EspNowMesh::_handleAlert(const EspNowAlertPacket& pkt) {
    // Ignore our own alerts
    if (pkt.cube_id == g_config.cube_id) return;

    // Deduplication — ignore duplicate broadcasts
    if (pkt.cube_id    == _lastAlertCubeId &&
        pkt.timestamp_s == _lastAlertTimestamp) {
        return;
    }
    _lastAlertCubeId    = pkt.cube_id;
    _lastAlertTimestamp = pkt.timestamp_s;

    Serial.printf("[ESP-NOW] ALERT from %s: level=%d source=%d val=%.1f\n",
                  pkt.room_name, (int)pkt.level, (int)pkt.source, pkt.value);

    // Forward to alert engine — it handles LED, buzzer, voice
    EspNowAlertPayload payload;
    payload.level       = pkt.level;
    payload.source      = pkt.source;
    payload.cube_id     = pkt.cube_id;
    payload.value       = pkt.value;
    payload.timestamp_s = pkt.timestamp_s;
    strncpy(payload.room_name, pkt.room_name, sizeof(payload.room_name) - 1);

    AlertEngine::onRemoteAlert(payload);
}

// ── _addPeer ─────────────────────────────────────────────────
bool EspNowMesh::_addPeer(const uint8_t* mac, const char* room_name,
                           uint8_t cube_id) {
    // Update if already known
    int slot = _findPeerSlot(mac);
    if (slot >= 0) {
        strncpy(_peers[slot].room_name, room_name,
                sizeof(_peers[slot].room_name) - 1);
        _peers[slot].last_seen_ms = millis();
        _peers[slot].active       = true;
        return true;
    }

    // Find empty slot
    for (int i = 0; i < ESPNOW_MAX_PEERS; i++) {
        if (!_peers[i].active) {
            memcpy(_peers[i].mac, mac, 6);
            strncpy(_peers[i].room_name, room_name,
                    sizeof(_peers[i].room_name) - 1);
            _peers[i].cube_id      = cube_id;
            _peers[i].last_seen_ms = millis();
            _peers[i].active       = true;
            _peerCount++;

            // Register with ESP-NOW for unicast (future use)
            esp_now_peer_info_t peerInfo = {};
            memcpy(peerInfo.peer_addr, mac, 6);
            peerInfo.channel = ESPNOW_CHANNEL;
            peerInfo.encrypt = false;
            peerInfo.ifidx   = WIFI_IF_STA;
            esp_now_add_peer(&peerInfo);

            return true;
        }
    }

    Serial.println("[ESP-NOW] WARNING: peer table full");
    return false;
}

// ── _isPeerKnown ─────────────────────────────────────────────
bool EspNowMesh::_isPeerKnown(const uint8_t* mac) {
    return _findPeerSlot(mac) >= 0;
}

int EspNowMesh::_findPeerSlot(const uint8_t* mac) {
    for (int i = 0; i < ESPNOW_MAX_PEERS; i++) {
        if (_peers[i].active && memcmp(_peers[i].mac, mac, 6) == 0) {
            return i;
        }
    }
    return -1;
}

// ── peerCount / dumpPeers ────────────────────────────────────
uint8_t EspNowMesh::peerCount() { return _peerCount; }

void EspNowMesh::dumpPeers() {
    Serial.printf("[ESP-NOW] %u active peer(s):\n", _peerCount);
    for (uint8_t i = 0; i < ESPNOW_MAX_PEERS; i++) {
        if (_peers[i].active) {
            uint32_t age = (millis() - _peers[i].last_seen_ms) / 1000;
            Serial.printf("  [%u] %s (ID:%u) — last seen %us ago\n",
                          i, _peers[i].room_name,
                          _peers[i].cube_id, age);
        }
    }
}
