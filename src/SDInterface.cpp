/*=============================================================
 * SDInterface.cpp — SD card operations
 *=============================================================*/
#include "SDInterface.h"
#include <Update.h>

SDInterface sdCard;

bool SDInterface::begin() {
    #if HAS_SD
    if (!SD.begin(SD_CS)) {
        Serial.println(F("[SD] Mount failed"));
        _available = false;
        return false;
    }
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("[SD] Card: %lluMB\n", cardSize);
    _available = true;
    ensureDirectories();
    return true;
    #else
    _available = false;
    return false;
    #endif
}

uint64_t SDInterface::getFreeSpace() {
    if (!_available) return 0;
    return SD.totalBytes() - SD.usedBytes();
}

void SDInterface::ensureDirectories() {
    if (!_available) return;
    const char* dirs[] = {"/wifi", "/bt", "/pcap", "/firmware", "/logs"};
    for (auto d : dirs) {
        if (!SD.exists(d)) SD.mkdir(d);
    }
}

String SDInterface::getDateString() {
    // Use millis-based date approximation (no RTC)
    // In production, this would use NTP or GPS time
    uint32_t days = millis() / 86400000UL;
    char buf[12];
    snprintf(buf, sizeof(buf), "20260101"); // placeholder date
    return String(buf);
}

String SDInterface::getTimeString() {
    uint32_t ms = millis();
    uint8_t h = (ms / 3600000) % 24;
    uint8_t m = (ms / 60000) % 60;
    uint8_t s = (ms / 1000) % 60;
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d%02d%02d", h, m, s);
    return String(buf);
}

String SDInterface::getTimestamp() {
    uint32_t ms = millis();
    uint32_t sec = ms / 1000;
    uint8_t h = (sec / 3600) % 24;
    uint8_t m = (sec / 60) % 60;
    uint8_t s = sec % 60;
    char buf[12];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", h, m, s);
    return String(buf);
}

void SDInterface::appendLine(const char* path, const String& line) {
    if (!_available) return;
    File f = SD.open(path, FILE_APPEND);
    if (f) {
        f.println(line);
        f.close();
    }
}

bool SDInterface::logAP(const char* bssid, const char* ssid, uint8_t ch,
                         int8_t rssi, const char* security) {
    if (!_available) return false;
    String date = getDateString();
    String path = "/wifi/aps_" + date + ".txt";
    String line = getTimestamp() + "," + String(bssid) + "," +
                  String(ssid) + "," + String(ch) + "," +
                  String(rssi) + "," + String(security);
    appendLine(path.c_str(), line);
    return true;
}

bool SDInterface::logStation(const char* mac, const char* apMac, int8_t rssi) {
    if (!_available) return false;
    String date = getDateString();
    String path = "/wifi/stations_" + date + ".txt";
    String line = getTimestamp() + "," + String(mac) + "," +
                  String(apMac) + "," + String(rssi);
    appendLine(path.c_str(), line);
    return true;
}

bool SDInterface::logDeauth(const char* src, const char* dst, const char* bssid) {
    if (!_available) return false;
    String date = getDateString();
    String path = "/wifi/deauths_" + date + ".txt";
    String line = getTimestamp() + "," + String(src) + "," +
                  String(dst) + "," + String(bssid);
    appendLine(path.c_str(), line);
    return true;
}

bool SDInterface::logBTDevice(const char* mac, const char* name,
                               int8_t rssi, const char* type) {
    if (!_available) return false;
    String date = getDateString();
    String path = "/bt/devices_" + date + ".txt";
    String line = getTimestamp() + "," + String(mac) + "," +
                  String(name) + "," + String(rssi) + "," + String(type);
    appendLine(path.c_str(), line);
    return true;
}

bool SDInterface::openPcap() {
    if (!_available || _pcapOpen) return false;
    String path = "/pcap/session_" + getDateString() + "_" + getTimeString() + ".pcap";
    _pcapFile = SD.open(path, FILE_WRITE);
    if (!_pcapFile) return false;

    PcapGlobalHeader ghdr;
    _pcapFile.write((uint8_t*)&ghdr, sizeof(ghdr));
    _pcapFile.flush();
    _pcapOpen = true;
    Serial.printf("[SD] PCAP opened: %s\n", path.c_str());
    return true;
}

bool SDInterface::writePcapPacket(const uint8_t* data, uint16_t len) {
    if (!_pcapOpen || !_pcapFile) return false;

    uint32_t ts = millis() / 1000;
    uint32_t ts_usec = (millis() % 1000) * 1000;

    PcapPacketHeader phdr;
    phdr.ts_sec = ts;
    phdr.ts_usec = ts_usec;
    phdr.incl_len = len;
    phdr.orig_len = len;

    _pcapFile.write((uint8_t*)&phdr, sizeof(phdr));
    _pcapFile.write(data, len);
    _pcapFile.flush();
    return true;
}

void SDInterface::closePcap() {
    if (_pcapOpen && _pcapFile) {
        _pcapFile.close();
        _pcapOpen = false;
        Serial.println(F("[SD] PCAP closed"));
    }
}

bool SDInterface::saveList(const char* path, const String items[], uint8_t count) {
    if (!_available) return false;
    File f = SD.open(path, FILE_WRITE);
    if (!f) return false;
    for (uint8_t i = 0; i < count; i++) {
        f.println(items[i]);
    }
    f.close();
    return true;
}

uint8_t SDInterface::loadList(const char* path, String items[], uint8_t maxCount) {
    if (!_available || !SD.exists(path)) return 0;
    File f = SD.open(path, FILE_READ);
    if (!f) return 0;
    uint8_t count = 0;
    while (f.available() && count < maxCount) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() > 0) {
            items[count++] = line;
        }
    }
    f.close();
    return count;
}

bool SDInterface::saveSSIDs(const String ssids[], uint8_t count) {
    return saveList("/ssids.txt", ssids, count);
}

uint8_t SDInterface::loadSSIDs(String ssids[], uint8_t maxCount) {
    return loadList("/ssids.txt", ssids, maxCount);
}

bool SDInterface::saveAPs(const String aps[], uint8_t count) {
    return saveList("/aps.txt", aps, count);
}

uint8_t SDInterface::loadAPs(String aps[], uint8_t maxCount) {
    return loadList("/aps.txt", aps, maxCount);
}

bool SDInterface::saveSettings(const String& json) {
    if (!_available) return false;
    File f = SD.open("/settings.json", FILE_WRITE);
    if (!f) return false;
    f.print(json);
    f.close();
    return true;
}

String SDInterface::loadSettings() {
    if (!_available || !SD.exists("/settings.json")) return "";
    File f = SD.open("/settings.json", FILE_READ);
    if (!f) return "";
    String json = f.readString();
    f.close();
    return json;
}

bool SDInterface::hasFirmwareUpdate() {
    return _available && SD.exists(OTA_SD_PATH);
}

bool SDInterface::applyFirmwareUpdate() {
    if (!hasFirmwareUpdate()) return false;

    File fw = SD.open(OTA_SD_PATH, FILE_READ);
    if (!fw) return false;

    size_t fwSize = fw.size();
    if (fwSize == 0) { fw.close(); return false; }

    Serial.printf("[OTA] Applying SD update: %u bytes\n", fwSize);

    if (!Update.begin(fwSize)) {
        Serial.println(F("[OTA] Not enough space"));
        fw.close();
        return false;
    }

    uint8_t buf[512];
    size_t written = 0;
    while (fw.available()) {
        size_t bytesRead = fw.read(buf, sizeof(buf));
        Update.write(buf, bytesRead);
        written += bytesRead;
    }
    fw.close();

    if (Update.end(true)) {
        Serial.println(F("[OTA] Update success, rebooting..."));
        SD.remove(OTA_SD_PATH);
        delay(500);
        ESP.restart();
        return true;
    } else {
        Serial.printf("[OTA] Update failed: %s\n", Update.errorString());
        return false;
    }
}

void SDInterface::rotateLogs() {
    // Simple rotation: remove files older than 30 days
    // Since we don't have RTC, this is a best-effort approach
    // In production, NTP timestamps would be used
    if (!_available) return;
    Serial.println(F("[SD] Log rotation check (placeholder)"));
}
