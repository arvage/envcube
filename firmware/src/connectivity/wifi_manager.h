#pragma once
// ============================================================
//  EnvCube — WiFi manager
//  Handles: captive portal provisioning, connection, reconnect.
//
//  Phase 1: Captive portal (no app required)
//  Phase 2: BLE provisioning via native app (future)
// ============================================================

#include <Arduino.h>

enum class WifiState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    PROVISIONING,   // AP + captive portal active
    FAILED
};

class WifiManager {
public:
    // Call once from setup(). Connects if credentials exist,
    // otherwise enters provisioning mode.
    static void begin();

    // Call from main loop. Handles reconnect, background tasks.
    static void loop();

    // Force re-provisioning (clears saved creds, starts AP).
    static void startProvisioning();

    // Trigger a graceful reconnect attempt.
    static void reconnect();

    // Current connection state.
    static WifiState state();

    // True if connected and IP assigned.
    static bool isConnected();

    // Returns assigned IP as string, or "" if not connected.
    static String ipAddress();

    // WiFi RSSI (signal strength), or 0 if not connected.
    static int rssi();

private:
    static void _connectWithCredentials();
    static void _startCaptivePortal();
    static void _onProvisionComplete(const char* ssid,
                                     const char* password,
                                     const char* room_name,
                                     float lat, float lon,
                                     const char* mqtt_host);
    static WifiState _state;
    static unsigned long _lastReconnectAttempt;
};
