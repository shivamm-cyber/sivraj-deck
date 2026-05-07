/*=============================================================
 * WiFiScan.h — Core WiFi engine: promiscuous, sniffing, attacks
 *=============================================================*/
#ifndef WIFI_SCAN_H
#define WIFI_SCAN_H

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include "configs.h"
#include "Buffer.h"

// Access point info
struct APInfo {
    uint8_t  bssid[6];
    char     ssid[33];
    uint8_t  channel;
    int8_t   rssi;
    uint8_t  encType;   // WIFI_AUTH_*
    bool     selected;  // marked for targeting
    uint32_t lastSeen;
};

// Station info
struct StationInfo {
    uint8_t  mac[6];
    uint8_t  apBssid[6]; // associated AP
    int8_t   rssi;
    bool     selected;
    uint32_t lastSeen;
};

// Deauth event
struct DeauthEvent {
    uint8_t src[6];
    uint8_t dst[6];
    uint8_t bssid[6];
    uint32_t timestamp;
};

class WiFiScanEngine {
public:
    WiFiScanEngine();
    void begin();
    void shutdown();

    // ── Scan operations ─────────────────────────
    void startScan(WiFiScanMode mode);
    void stopScan();
    bool isScanning() const { return _currentMode != WIFI_SCAN_OFF; }
    WiFiScanMode getCurrentMode() const { return _currentMode; }

    // ── Channel control ─────────────────────────
    void setChannel(uint8_t ch);
    uint8_t getChannel() const { return _currentChannel; }
    void startChannelHop(uint16_t intervalMs = 500);
    void stopChannelHop();

    // ── AP scanning ─────────────────────────────
    void scanAPs();
    void scanStations();
    void scanAll();

    // ── Sniffers ────────────────────────────────
    void startProbeSniff();
    void startDeauthSniff();
    void startBeaconSniff();
    void startEAPOLSniff();
    void startPacketMonitor();
    void startRawSniff();
    void startPineappleSniff();
    void startMultiSSIDSniff();
    void startChannelAnalyzer();

    // ── Network scanners ────────────────────────
    void startPingScan(IPAddress subnet);
    void startARPScan();
    void startPortScan(IPAddress target, uint16_t startPort, uint16_t endPort);
    void startSSHScan();
    void startTelnetScan();

    // ── Attacks ─────────────────────────────────
    void startDeauthFlood();
    void startDeauthTargeted(uint8_t* bssid, uint8_t* staMac);
    void startBeaconSpamList();
    void startBeaconSpamRandom();
    void startRickRoll();
    void startProbeFlood();
    void startAPCloneSpam();

    // ── General ops ─────────────────────────────
    void selectAP(uint8_t index);
    void selectStation(uint8_t index);
    void clearAPs();
    void clearStations();
    void clearSSIDs();
    void addSSID(const char* ssid);
    void generateSSIDs(uint8_t count = 20);
    bool setMAC(const uint8_t* mac);
    bool joinWiFi(const char* ssid, const char* pass);
    void disconnectWiFi();

    // ── Data access ─────────────────────────────
    APInfo*      getAPs()      { return _aps; }
    StationInfo* getStations() { return _stations; }
    String*      getSSIDs()    { return _ssids; }
    uint16_t     getAPCount()  const { return _apCount; }
    uint16_t     getStationCount() const { return _stCount; }
    uint8_t      getSSIDCount() const { return _ssidCount; }
    uint32_t     getPacketsPerSec() const { return _pps; }
    uint32_t     getTotalPackets() const { return _totalPkts; }
    uint8_t      getHandshakes() const { return _handshakes; }
    uint32_t     getEapolCount() const { return _eapolCount; }
    float*       getChannelData() { return _channelPkts; }
    uint32_t     getDeauthCount() const { return _deauthCount; }
    uint32_t     getAttackPktCount() const { return _attackPktsSent; }

    // ── Loop/task update ────────────────────────
    void update();

    // ── Packet buffer ───────────────────────────
    PacketBuffer packetBuf;

private:
    WiFiScanMode _currentMode = WIFI_SCAN_OFF;
    uint8_t  _currentChannel = 1;
    bool     _channelHopping = false;
    uint16_t _channelHopMs = 500;
    uint32_t _lastChannelHop = 0;

    // AP storage
    APInfo   _aps[MAX_APS];
    uint16_t _apCount = 0;

    // Station storage
    StationInfo _stations[MAX_STATIONS];
    uint16_t    _stCount = 0;

    // SSID list
    String  _ssids[MAX_SSIDS];
    uint8_t _ssidCount = 0;

    // Packet counters
    volatile uint32_t _pktCount = 0;
    uint32_t _lastPpsCalc = 0;
    uint32_t _lastPktSnapshot = 0;
    uint32_t _pps = 0;
    uint32_t _totalPkts = 0;

    // EAPOL
    uint8_t  _handshakes = 0;
    uint32_t _eapolCount = 0;

    // Deauth detection
    uint32_t _deauthCount = 0;

    // Attack counters
    uint32_t _attackPktsSent = 0;
    uint32_t _attackStartTime = 0;
    bool     _attackRunning = false;

    // Channel analyzer
    float _channelPkts[14] = {0};

    // Promiscuous mode callback
    static void promiscuousCallback(void* buf, wifi_promiscuous_pkt_type_t type);
    static WiFiScanEngine* _instance;

    // Internal helpers
    void enablePromiscuous();
    void disablePromiscuous();
    bool addAP(const uint8_t* bssid, const char* ssid, uint8_t ch,
               int8_t rssi, uint8_t enc);
    bool addStation(const uint8_t* mac, const uint8_t* apBssid, int8_t rssi);
    void parsePacket(const uint8_t* payload, uint16_t len,
                     int8_t rssi, uint8_t ch);
    void sendDeauth(const uint8_t* bssid, const uint8_t* dst, uint8_t ch);
    void sendBeacon(const char* ssid, uint8_t ch, const uint8_t* srcMac);
    void sendProbeRequest(const char* ssid, uint8_t ch);
    void hopChannel();
    String macToString(const uint8_t* mac);
    void generateRandomMAC(uint8_t* mac);
    void generateRandomSSID(char* buf, uint8_t maxLen);
};

extern WiFiScanEngine wifiEngine;

#endif // WIFI_SCAN_H
