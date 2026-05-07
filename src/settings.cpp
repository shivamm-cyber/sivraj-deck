/*=============================================================
 * settings.cpp — JSON configuration persistence
 *=============================================================*/
#include "settings.h"
#include <ArduinoJson.h>
#include <FS.h>
#include <SD.h>

const char* SettingsManager::SETTINGS_FILE = "/settings.json";

SettingsManager settingsMgr;

SettingsManager::SettingsManager() {
    settings.setDefaults();
}

bool SettingsManager::begin() {
    if (!load()) {
        Serial.println(F("[SETTINGS] No config found, using defaults"));
        settings.setDefaults();
        return false;
    }
    return true;
}

bool SettingsManager::load() {
    if (!SD.exists(SETTINGS_FILE)) {
        return false;
    }

    File f = SD.open(SETTINGS_FILE, FILE_READ);
    if (!f) {
        Serial.println(F("[SETTINGS] Failed to open settings file"));
        return false;
    }

    String json = f.readString();
    f.close();

    return fromJson(json);
}

bool SettingsManager::save() {
    String json = toJson();

    File f = SD.open(SETTINGS_FILE, FILE_WRITE);
    if (!f) {
        Serial.println(F("[SETTINGS] Failed to write settings file"));
        return false;
    }

    f.print(json);
    f.close();
    Serial.println(F("[SETTINGS] Saved"));
    return true;
}

void SettingsManager::resetDefaults() {
    settings.setDefaults();
    save();
}

String SettingsManager::toJson() {
    JsonDocument doc;

    doc["wifi_ssid"]       = settings.wifi_ssid;
    doc["wifi_pass"]       = settings.wifi_pass;
    doc["mqtt_server"]     = settings.mqtt_server;
    doc["mqtt_port"]       = settings.mqtt_port;
    doc["mqtt_user"]       = settings.mqtt_user;
    doc["mqtt_pass"]       = settings.mqtt_pass;
    doc["mqtt_enabled"]    = settings.mqtt_enabled;
    doc["brightness"]      = settings.brightness;
    doc["theme"]           = settings.theme;
    doc["default_channel"] = settings.default_channel;
    doc["auto_ch_hop"]     = settings.auto_channel_hop;
    doc["ch_hop_ms"]       = settings.channel_hop_ms;
    doc["device_name"]     = settings.device_name;
    doc["gps_enabled"]     = settings.gps_enabled;
    doc["sd_logging"]      = settings.sd_logging;
    doc["led_mode"]        = settings.led_mode;
    doc["sound"]           = settings.sound_enabled;
    doc["use_custom_mac"]  = settings.use_custom_mac;

    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             settings.custom_mac[0], settings.custom_mac[1],
             settings.custom_mac[2], settings.custom_mac[3],
             settings.custom_mac[4], settings.custom_mac[5]);
    doc["custom_mac"] = macStr;

    String out;
    serializeJsonPretty(doc, out);
    return out;
}

bool SettingsManager::fromJson(const String& json) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) {
        Serial.printf("[SETTINGS] Parse error: %s\n", err.c_str());
        return false;
    }

    if (doc["wifi_ssid"].is<const char*>())
        strlcpy(settings.wifi_ssid, doc["wifi_ssid"] | "", sizeof(settings.wifi_ssid));
    if (doc["wifi_pass"].is<const char*>())
        strlcpy(settings.wifi_pass, doc["wifi_pass"] | "", sizeof(settings.wifi_pass));
    if (doc["mqtt_server"].is<const char*>())
        strlcpy(settings.mqtt_server, doc["mqtt_server"] | MQTT_DEFAULT_SERVER, sizeof(settings.mqtt_server));
    if (doc["mqtt_port"].is<int>())
        settings.mqtt_port = doc["mqtt_port"] | MQTT_DEFAULT_PORT;
    if (doc["mqtt_user"].is<const char*>())
        strlcpy(settings.mqtt_user, doc["mqtt_user"] | "", sizeof(settings.mqtt_user));
    if (doc["mqtt_pass"].is<const char*>())
        strlcpy(settings.mqtt_pass, doc["mqtt_pass"] | "", sizeof(settings.mqtt_pass));
    if (doc["mqtt_enabled"].is<bool>())
        settings.mqtt_enabled = doc["mqtt_enabled"] | true;
    if (doc["brightness"].is<int>())
        settings.brightness = doc["brightness"] | 200;
    if (doc["theme"].is<int>())
        settings.theme = doc["theme"] | 0;
    if (doc["default_channel"].is<int>())
        settings.default_channel = doc["default_channel"] | 1;
    if (doc["auto_ch_hop"].is<bool>())
        settings.auto_channel_hop = doc["auto_ch_hop"] | true;
    if (doc["ch_hop_ms"].is<int>())
        settings.channel_hop_ms = doc["ch_hop_ms"] | 500;
    if (doc["device_name"].is<const char*>())
        strlcpy(settings.device_name, doc["device_name"] | "sivraj-deck", sizeof(settings.device_name));
    if (doc["gps_enabled"].is<bool>())
        settings.gps_enabled = doc["gps_enabled"] | false;
    if (doc["sd_logging"].is<bool>())
        settings.sd_logging = doc["sd_logging"] | true;
    if (doc["led_mode"].is<int>())
        settings.led_mode = doc["led_mode"] | 1;
    if (doc["sound"].is<bool>())
        settings.sound_enabled = doc["sound"] | false;
    if (doc["use_custom_mac"].is<bool>())
        settings.use_custom_mac = doc["use_custom_mac"] | false;

    if (doc["custom_mac"].is<const char*>()) {
        const char* macStr = doc["custom_mac"];
        if (macStr && strlen(macStr) == 17) {
            sscanf(macStr, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                   &settings.custom_mac[0], &settings.custom_mac[1],
                   &settings.custom_mac[2], &settings.custom_mac[3],
                   &settings.custom_mac[4], &settings.custom_mac[5]);
        }
    }

    Serial.println(F("[SETTINGS] Loaded from JSON"));
    return true;
}
