/*=============================================================
 * SDInterface.h — SD card operations: logging, PCAP, config
 *=============================================================*/
#ifndef SD_INTERFACE_H
#define SD_INTERFACE_H

#include <Arduino.h>
#include <SD.h>
#include <FS.h>
#include "configs.h"

// PCAP global header
struct PcapGlobalHeader {
    uint32_t magic_number  = 0xa1b2c3d4;
    uint16_t version_major = 2;
    uint16_t version_minor = 4;
    int32_t  thiszone      = 0;
    uint32_t sigfigs       = 0;
    uint32_t snaplen       = 65535;
    uint32_t network       = 105;  // LINKTYPE_IEEE802_11
};

// PCAP packet header
struct PcapPacketHeader {
    uint32_t ts_sec;
    uint32_t ts_usec;
    uint32_t incl_len;
    uint32_t orig_len;
};

class SDInterface {
public:
    bool begin();
    bool isAvailable() const { return _available; }
    bool isReady() const { return _available; }  // alias
    uint64_t getFreeSpace();

    // Directory setup
    void ensureDirectories();

    // Logging
    bool logAP(const char* bssid, const char* ssid, uint8_t ch,
               int8_t rssi, const char* security);
    bool logStation(const char* mac, const char* apMac, int8_t rssi);
    bool logDeauth(const char* src, const char* dst, const char* bssid);
    bool logBTDevice(const char* mac, const char* name, int8_t rssi,
                     const char* type);

    // PCAP
    bool openPcap();
    bool writePcapPacket(const uint8_t* data, uint16_t len);
    void closePcap();
    bool isPcapOpen() const { return _pcapOpen; }

    // SSID/AP list management
    bool saveSSIDs(const String ssids[], uint8_t count);
    uint8_t loadSSIDs(String ssids[], uint8_t maxCount);
    bool saveAPs(const String aps[], uint8_t count);
    uint8_t loadAPs(String aps[], uint8_t maxCount);

    // Settings
    bool saveSettings(const String& json);
    String loadSettings();

    // Firmware update from SD
    bool hasFirmwareUpdate();
    bool applyFirmwareUpdate();

    // Log rotation
    void rotateLogs();

private:
    bool _available = false;
    bool _pcapOpen = false;
    File _pcapFile;
    String _currentDate;

    String getDateString();
    String getTimeString();
    String getTimestamp();
    void appendLine(const char* path, const String& line);
    bool saveList(const char* path, const String items[], uint8_t count);
    uint8_t loadList(const char* path, String items[], uint8_t maxCount);
};

extern SDInterface sdCard;

#endif // SD_INTERFACE_H
