#pragma once
// ============================================================
//  EnvCube — ESP-NOW encrypted mesh
//  AES-128 via PMK/LMK · peer-to-peer · no router needed
//
//  How it works:
//  1. On boot each cube broadcasts a HELLO packet with its
//     room name and MAC address.
//  2. Peers that receive HELLO auto-register each other.
//  3. On a CRITICAL/ALERT event the detecting cube broadcasts
//     an ALERT payload to all registered peers.
//  4. Peers alarm locally, speaking the originating room name.
//  5. All messages AES-128 encrypted with shared PMK.
//
//  Security:
//  - PMK (Primary Master Key) shared across all cubes in a
//    home — set in config.h, change before production.
//  - LMK (Local Master Key) per peer, derived from PMK.
//  - Neighbour cubes (different homes) cannot inject alerts.
//
//  Range: ~50m through walls (24GHz WiFi channel).
//  Latency: <10ms — works with no internet, no router.
// ============================================================

#include <Arduino.h>
#include "../alert_types.h"

// ── Packet types ─────────────────────────────────────────────
enum class EspNowPacketType : uint8_t {
    HELLO  = 0x01,   // Announce presence on boot / periodically
    ALERT  = 0x02,   // Critical/alert event notification
    ACK    = 0x03,   // Acknowledgement (future use)
};

// ── HELLO packet ─────────────────────────────────────────────
struct EspNowHelloPacket {
    EspNowPacketType type     = EspNowPacketType::HELLO;
    uint8_t          version  = 1;
    uint8_t          cube_id;
    char             room_name[32];
    uint8_t          mac[6];
};

// ── ALERT packet ─────────────────────────────────────────────
struct EspNowAlertPacket {
    EspNowPacketType type     = EspNowPacketType::ALERT;
    uint8_t          version  = 1;
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

class EspNowMesh {
public:
    // Initialise ESP-NOW, set PMK, register receive callback.
    static bool begin();

    // Broadcast an alert to all registered peers.
    // Called by AlertEngine when level >= ALERT.
    static bool broadcastAlert(AlertLevel level, AlertSource source,
                               const char* room_name, float value);

    // Send HELLO to all peers (called on boot + every 60s).
    static void broadcastHello();

    // Call from connectivity task loop.
    static void loop();

    // Number of currently active peers.
    static uint8_t peerCount();

    // Print peer list to Serial.
    static void dumpPeers();

private:
    // ESP-NOW receive callback (must be static, called from ISR context)
    static void _onReceive(const uint8_t* mac, const uint8_t* data, int len);

    static void _handleHello(const uint8_t* mac,
                             const EspNowHelloPacket& pkt);
    static void _handleAlert(const EspNowAlertPacket& pkt);

    static bool _addPeer(const uint8_t* mac, const char* room_name,
                         uint8_t cube_id);
    static bool _isPeerKnown(const uint8_t* mac);
    static int  _findPeerSlot(const uint8_t* mac);

    static EspNowPeer _peers[ESPNOW_MAX_PEERS];
    static uint8_t    _peerCount;
    static uint32_t   _lastHelloMs;
    static bool       _ready;
    static uint8_t    _myMac[6];

    // Deduplication: ignore alerts with same cube_id+timestamp
    // seen within the last 5 seconds
    static uint8_t   _lastAlertCubeId;
    static uint32_t  _lastAlertTimestamp;

    static const uint32_t HELLO_INTERVAL_MS  = 60000;
    static const uint32_t PEER_TIMEOUT_MS    = 300000; // 5 min
};
