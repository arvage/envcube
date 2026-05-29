#pragma once
// ============================================================
//  EnvCube — ESP-NOW encrypted mesh header
// ============================================================

#include <Arduino.h>
#include "../config.h"       // MUST come before use of ESPNOW_MAX_PEERS
#include "../alert_types.h"
#include <esp_now.h>

// ── Packet types ─────────────────────────────────────────────
enum class EspNowPacketType : uint8_t {
    HELLO = 0x01,
    ALERT = 0x02,
    ACK   = 0x03,
};

// ── HELLO packet ─────────────────────────────────────────────
struct EspNowHelloPacket {
    EspNowPacketType type    = EspNowPacketType::HELLO;
    uint8_t          version = 1;
    uint8_t          cube_id;
    char             room_name[32];
    uint8_t          mac[6];
};

// ── ALERT packet ─────────────────────────────────────────────
struct EspNowAlertPacket {
    EspNowPacketType type    = EspNowPacketType::ALERT;
    uint8_t          version = 1;
    AlertLevel       level;
    AlertSource      source;
    uint8_t          cube_id;
    char             room_name[32];
    float            value;
    uint32_t         timestamp_s;
};

// ── Peer record ──────────────────────────────────────────────
struct EspNowPeer {
    uint8_t  mac[6];
    char     room_name[32];
    uint8_t  cube_id;
    uint32_t last_seen_ms;
    bool     active;
};

// ── Max peers as a plain constant (not macro) so it's
//    available at class definition time regardless of
//    include order ──────────────────────────────────────────
static const uint8_t ESPNOW_PEER_MAX = 10;

class EspNowMesh {
public:
    static bool    begin();
    static bool    broadcastAlert(AlertLevel level, AlertSource source,
                                  const char* room_name, float value);
    static void    broadcastHello();
    static void    loop();
    static uint8_t peerCount();
    static void    dumpPeers();

private:
    // ESP-NOW Core 3.x recv callback signature
    static void _onReceive(const esp_now_recv_info* info,
                           const uint8_t* data, int len);

    static void _handleHello(const uint8_t* mac,
                             const EspNowHelloPacket& pkt);
    static void _handleAlert(const EspNowAlertPacket& pkt);
    static bool _addPeer(const uint8_t* mac, const char* room_name,
                         uint8_t cube_id);
    static bool _isPeerKnown(const uint8_t* mac);
    static int  _findPeerSlot(const uint8_t* mac);

    // Fixed-size array using the constant (not the macro)
    static EspNowPeer _peers[ESPNOW_PEER_MAX];
    static uint8_t    _peerCount;
    static uint32_t   _lastHelloMs;
    static bool       _ready;
    static uint8_t    _myMac[6];
    static uint8_t    _lastAlertCubeId;
    static uint32_t   _lastAlertTimestamp;

    static const uint32_t HELLO_INTERVAL_MS = 60000;
    static const uint32_t PEER_TIMEOUT_MS   = 300000;
};
