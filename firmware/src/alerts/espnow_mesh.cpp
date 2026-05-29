// ============================================================
//  EnvCube — ESP-NOW encrypted mesh (full implementation)
//  Updated for Arduino ESP32 Core 3.x:
//  - esp_now_recv_cb_t signature changed to recv_info_t*
//  - Uses ESPNOW_PEER_MAX constant (not ESPNOW_MAX_PEERS macro)
// ============================================================

#include "espnow_mesh.h"
#include "alert_engine.h"
#include "../storage/nvs_config.h"
#include <WiFi.h>
#include <esp_now.h>

// ── Static member definitions ────────────────────────────────
EspNowPeer EspNowMesh::_peers[ESPNOW_PEER_MAX] = {};
uint8_t    EspNowMesh::_peerCount           = 0;
uint32_t   EspNowMesh::_lastHelloMs         = 0;
bool       EspNowMesh::_ready               = false;
uint8_t    EspNowMesh::_myMac[6]            = {};
uint8_t    EspNowMesh::_lastAlertCubeId     = 0xFF;
uint32_t   EspNowMesh::_lastAlertTimestamp  = 0;

// ── ISR-safe ring buffer ─────────────────────────────────────
struct RxEntry {
    uint8_t mac[6];
    uint8_t data[250];
    int     len;
    bool    ready;
};
static volatile RxEntry _rxBuf[4];
static volatile uint8_t _rxWrite = 0;
static          uint8_t _rxRead  = 0;

// ── begin ────────────────────────────────────────────────────
bool EspNowMesh::begin() {
    if (WiFi.getMode() == WIFI_OFF) WiFi.mode(WIFI_STA);

    WiFi.macAddress(_myMac);
    Serial.printf("[ESP-NOW] MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  _myMac[0],_myMac[1],_myMac[2],
                  _myMac[3],_myMac[4],_myMac[5]);

    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESP-NOW] ERROR: init failed");
        return false;
    }

    // PMK — AES-128 (16 bytes)
    uint8_t pmk[16];
    const char* s = ESPNOW_PMK;
    size_t slen = strlen(s);
    for (int i = 0; i < 16; i++) pmk[i] = (uint8_t)s[i % slen];
    esp_now_set_pmk(pmk);

    // Core 3.x callback signature: (recv_info_t*, data, len)
    esp_now_register_recv_cb(_onReceive);

    // Register broadcast peer
    uint8_t bcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    esp_now_peer_info_t pi = {};
    memcpy(pi.peer_addr, bcast, 6);
    pi.channel = ESPNOW_CHANNEL;
    pi.encrypt = false;
    pi.ifidx   = WIFI_IF_STA;
    esp_now_add_peer(&pi);

    _ready       = true;
    _lastHelloMs = millis();

    Serial.printf("[ESP-NOW] Ready — channel %d  max peers %d\n",
                  ESPNOW_CHANNEL, ESPNOW_PEER_MAX);
    broadcastHello();
    return true;
}

// ── broadcastAlert ───────────────────────────────────────────
bool EspNowMesh::broadcastAlert(AlertLevel level, AlertSource source,
                                 const char* room_name, float value) {
    if (!_ready) return false;

    EspNowAlertPacket pkt;
    pkt.level       = level;
    pkt.source      = source;
    pkt.cube_id     = g_config.cube_id;
    pkt.value       = value;
    pkt.timestamp_s = millis() / 1000;
    strncpy(pkt.room_name, room_name, sizeof(pkt.room_name)-1);

    uint8_t bcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    bool ok = true;
    for (uint8_t i = 0; i < ESPNOW_ALERT_REPEATS; i++) {
        if (esp_now_send(bcast,(const uint8_t*)&pkt,sizeof(pkt)) != ESP_OK)
            ok = false;
        if (i < ESPNOW_ALERT_REPEATS-1) delay(10);
    }
    Serial.printf("[ESP-NOW] Alert → level=%d src=%d room=%s\n",
                  (int)level,(int)source,room_name);
    return ok;
}

// ── broadcastHello ───────────────────────────────────────────
void EspNowMesh::broadcastHello() {
    if (!_ready) return;
    EspNowHelloPacket pkt;
    pkt.cube_id = g_config.cube_id;
    memcpy(pkt.mac, _myMac, 6);
    strncpy(pkt.room_name, g_config.room_name, sizeof(pkt.room_name)-1);
    uint8_t bcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    esp_now_send(bcast,(const uint8_t*)&pkt,sizeof(pkt));
    Serial.printf("[ESP-NOW] HELLO — room:%s id:%u\n",
                  g_config.room_name, g_config.cube_id);
}

// ── loop ─────────────────────────────────────────────────────
void EspNowMesh::loop() {
    if (!_ready) return;

    // Drain ring buffer
    while (_rxRead != _rxWrite) {
        uint8_t idx = _rxRead;
        if (_rxBuf[idx].ready) {
            const uint8_t* mac  = (const uint8_t*)_rxBuf[idx].mac;
            const uint8_t* data = (const uint8_t*)_rxBuf[idx].data;
            int             len  = _rxBuf[idx].len;
            if (len >= 1) {
                auto pktType = (EspNowPacketType)data[0];
                if (pktType == EspNowPacketType::HELLO &&
                    len >= (int)sizeof(EspNowHelloPacket)) {
                    EspNowHelloPacket p;
                    memcpy(&p, data, sizeof(p));
                    _handleHello(mac, p);
                } else if (pktType == EspNowPacketType::ALERT &&
                           len >= (int)sizeof(EspNowAlertPacket)) {
                    EspNowAlertPacket p;
                    memcpy(&p, data, sizeof(p));
                    _handleAlert(p);
                }
            }
            _rxBuf[idx].ready = false;
            _rxRead = (_rxRead + 1) % 4;
        } else break;
    }

    // Periodic HELLO
    if (millis() - _lastHelloMs >= HELLO_INTERVAL_MS) {
        broadcastHello();
        _lastHelloMs = millis();
    }

    // Timeout inactive peers
    for (uint8_t i = 0; i < ESPNOW_PEER_MAX; i++) {
        if (_peers[i].active &&
            millis() - _peers[i].last_seen_ms > PEER_TIMEOUT_MS) {
            Serial.printf("[ESP-NOW] Peer %s timed out\n", _peers[i].room_name);
            _peers[i].active = false;
            _peerCount = 0;
            for (uint8_t j = 0; j < ESPNOW_PEER_MAX; j++)
                if (_peers[j].active) _peerCount++;
        }
    }
}

// ── _onReceive (Core 3.x signature) ─────────────────────────
void EspNowMesh::_onReceive(const esp_now_recv_info_t* info,
                             const uint8_t* data, int len) {
    uint8_t idx = _rxWrite;
    if (!_rxBuf[idx].ready && len > 0) {
        memcpy((void*)_rxBuf[idx].mac, info->src_addr, 6);
        int n = (len > 250) ? 250 : len;
        memcpy((void*)_rxBuf[idx].data, data, n);
        _rxBuf[idx].len   = n;
        _rxBuf[idx].ready = true;
        _rxWrite = (_rxWrite + 1) % 4;
    }
}

// ── _handleHello ─────────────────────────────────────────────
void EspNowMesh::_handleHello(const uint8_t* mac,
                               const EspNowHelloPacket& pkt) {
    if (memcmp(mac, _myMac, 6) == 0) return; // ignore own broadcast
    bool known = _isPeerKnown(mac);
    _addPeer(mac, pkt.room_name, pkt.cube_id);
    if (!known) {
        Serial.printf("[ESP-NOW] New peer: %s (ID:%u)\n",
                      pkt.room_name, pkt.cube_id);
    } else {
        int slot = _findPeerSlot(mac);
        if (slot >= 0) _peers[slot].last_seen_ms = millis();
    }
}

// ── _handleAlert ─────────────────────────────────────────────
void EspNowMesh::_handleAlert(const EspNowAlertPacket& pkt) {
    if (pkt.cube_id == g_config.cube_id) return;
    if (pkt.cube_id    == _lastAlertCubeId &&
        pkt.timestamp_s == _lastAlertTimestamp) return; // duplicate
    _lastAlertCubeId    = pkt.cube_id;
    _lastAlertTimestamp = pkt.timestamp_s;

    Serial.printf("[ESP-NOW] ALERT from %s level=%d src=%d\n",
                  pkt.room_name,(int)pkt.level,(int)pkt.source);

    EspNowAlertPayload payload;
    payload.level       = pkt.level;
    payload.source      = pkt.source;
    payload.cube_id     = pkt.cube_id;
    payload.value       = pkt.value;
    payload.timestamp_s = pkt.timestamp_s;
    strncpy(payload.room_name, pkt.room_name, sizeof(payload.room_name)-1);
    AlertEngine::onRemoteAlert(payload);
}

// ── _addPeer ─────────────────────────────────────────────────
bool EspNowMesh::_addPeer(const uint8_t* mac, const char* room_name,
                           uint8_t cube_id) {
    int slot = _findPeerSlot(mac);
    if (slot >= 0) {
        strncpy(_peers[slot].room_name, room_name,
                sizeof(_peers[slot].room_name)-1);
        _peers[slot].last_seen_ms = millis();
        _peers[slot].active       = true;
        return true;
    }
    for (int i = 0; i < ESPNOW_PEER_MAX; i++) {
        if (!_peers[i].active) {
            memcpy(_peers[i].mac, mac, 6);
            strncpy(_peers[i].room_name, room_name,
                    sizeof(_peers[i].room_name)-1);
            _peers[i].cube_id      = cube_id;
            _peers[i].last_seen_ms = millis();
            _peers[i].active       = true;
            _peerCount++;
            // Register unicast peer for future targeted sends
            esp_now_peer_info_t pi = {};
            memcpy(pi.peer_addr, mac, 6);
            pi.channel = ESPNOW_CHANNEL;
            pi.encrypt = false;
            pi.ifidx   = WIFI_IF_STA;
            esp_now_add_peer(&pi);
            return true;
        }
    }
    Serial.println("[ESP-NOW] WARNING: peer table full");
    return false;
}

bool EspNowMesh::_isPeerKnown(const uint8_t* mac) {
    return _findPeerSlot(mac) >= 0;
}

int EspNowMesh::_findPeerSlot(const uint8_t* mac) {
    for (int i = 0; i < ESPNOW_PEER_MAX; i++)
        if (_peers[i].active && memcmp(_peers[i].mac, mac, 6) == 0)
            return i;
    return -1;
}

uint8_t EspNowMesh::peerCount() { return _peerCount; }

void EspNowMesh::dumpPeers() {
    Serial.printf("[ESP-NOW] %u peer(s):\n", _peerCount);
    for (uint8_t i = 0; i < ESPNOW_PEER_MAX; i++) {
        if (_peers[i].active) {
            uint32_t age = (millis() - _peers[i].last_seen_ms) / 1000;
            Serial.printf("  [%u] %s id:%u last:%us ago\n",
                          i,_peers[i].room_name,_peers[i].cube_id,age);
        }
    }
}
