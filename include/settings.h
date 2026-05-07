/*=============================================================
 * settings.h — JSON configuration persistence
 *=============================================================*/
#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include "configs.h"

struct DeviceSettings {
    // WiFi STA credentials (for MQTT connection)
    char wifi_ssid[33];
    char wifi_pass[65];

    // MQTT config
    char mqtt_server[64];
    uint16_t mqtt_port;
    char mqtt_user[32];
    char mqtt_pass[32];
    bool mqtt_enabled;

    // Display
    uint8_t brightness;      // 0-255
    uint8_t theme;           // 0=dark, 1=navy

    // WiFi scan defaults
    uint8_t default_channel;
    bool auto_channel_hop;
    uint16_t channel_hop_ms;

    // General
    char device_name[32];
    bool gps_enabled;
    bool sd_logging;
    uint8_t led_mode;       // 0=off, 1=status, 2=scan
    bool sound_enabled;

    // MAC
    uint8_t custom_mac[6];
    bool use_custom_mac;

    void setDefaults() {
        memset(wifi_ssid, 0, sizeof(wifi_ssid));
        memset(wifi_pass, 0, sizeof(wifi_pass));
        strncpy(mqtt_server, MQTT_DEFAULT_SERVER, sizeof(mqtt_server) - 1);
        mqtt_port = MQTT_DEFAULT_PORT;
        memset(mqtt_user, 0, sizeof(mqtt_user));
        memset(mqtt_pass, 0, sizeof(mqtt_pass));
        mqtt_enabled = true;
        brightness = 200;
        theme = 0;
        default_channel = 1;
        auto_channel_hop = true;
        channel_hop_ms = 500;
        strncpy(device_name, "sivraj-deck", sizeof(device_name) - 1);
        gps_enabled = false;
        sd_logging = true;
        led_mode = 1;
        sound_enabled = false;
        memset(custom_mac, 0, 6);
        use_custom_mac = false;
    }
};

class SettingsManager {
public:
    DeviceSettings settings;

    SettingsManager();
    bool begin();
    bool load();
    bool save();
    void resetDefaults();
    String toJson();
    bool fromJson(const String& json);

private:
    static const char* SETTINGS_FILE;
};

extern SettingsManager settingsMgr;

#endif // SETTINGS_H
