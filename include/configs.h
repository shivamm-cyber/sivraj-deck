#pragma once
#ifndef configs_h
#define configs_h

#include <Arduino.h>
#include <LinkedList.h>

#define FIRMWARE_VERSION "1.0.0"
#define FIRMWARE_NAME    "SIVRAJ-DECK"

// ── Display ─────────────────────────────────────────────────
#define SCREEN_WIDTH       320
#define SCREEN_HEIGHT      240
#define TFT_SCREEN_WIDTH   320
#define TFT_SCREEN_HEIGHT  240
#define TOP_FIXED_AREA     16
#define STATUS_BAR_H       16
#define HEADER_H           22
#define MENU_Y_START       (STATUS_BAR_H + HEADER_H)
#define MENU_ITEM_H        24
#define MENU_ITEMS_VISIBLE 8
#define STATUS_BAR_COLOR   0x000A

// ── Buttons ─────────────────────────────────────────────────
#define BTN_UP     34
#define BTN_SELECT 32
#define BTN_DOWN   35

// ── SD Card ─────────────────────────────────────────────────
#define SD_CS 4

// ── Battery ─────────────────────────────────────────────────
#define BATTERY_PIN     36
#define CHARGING_PIN    33
#define BATTERY_REF     3.3f
#define BATTERY_ADC_RES 4095
#define BATTERY_R1      100.0f
#define BATTERY_R2      47.0f
#define FULL_BAT_MV     4200
#define EMPTY_BAT_MV    3300
#define CRIT_BAT_MV     3200
#define LED_PIN         2

// ── MQTT ────────────────────────────────────────────────────
#define MQTT_DEFAULT_SERVER  "mqtt.sivraj.in"
#define MQTT_DEFAULT_PORT    1883
#define MQTT_BROKER          MQTT_DEFAULT_SERVER
#define MQTT_PORT            MQTT_DEFAULT_PORT
#define MQTT_TOPIC_STATUS    "sivraj/deck/status"
#define MQTT_TOPIC_ALERT     "sivraj/deck/alert"
#define MQTT_TOPIC_COMMAND   "sivraj/deck/command"
#define MQTT_PUBLISH_INTERVAL 5000
#define MQTT_MAX_QUEUE       100
#define MQTT_RECONNECT_BASE  1000
#define MQTT_RECONNECT_MAX   30000

// ── WiFi Scan ───────────────────────────────────────────────
#define HOP_DELAY    500
#define MAX_CHANNELS 13
#define SNAP_LEN     2324
#define MAX_APS      64
#define MAX_STATIONS 128
#define MAX_SSIDS    50

// ── OTA ─────────────────────────────────────────────────────
#define OTA_AP_PREFIX "SIVRAJ_DECK_"
#define OTA_SD_PATH   "/firmware/current.bin"

// ── WiFi credentials (for STA mode) ────────────────────────
#define WIFI_SSID  "SATYAM_PG_4G"
#define WIFI_PASS  "india@321"

// ── Scan Mode Enum ──────────────────────────────────────────
enum WiFiScanMode : uint8_t {
    WIFI_SCAN_OFF = 0,
    WIFI_SCAN_AP,
    WIFI_SCAN_STATION,
    WIFI_SCAN_ALL,
    WIFI_SCAN_PROBE,
    WIFI_SCAN_DEAUTH_SNIFF,
    WIFI_SCAN_BEACON_SNIFF,
    WIFI_SCAN_EAPOL,
    WIFI_SCAN_PACKET_MONITOR,
    WIFI_SCAN_RAW,
    WIFI_SCAN_PINEAPPLE,
    WIFI_SCAN_MULTI_SSID,
    WIFI_SCAN_CHANNEL_ANALYZER,
    WIFI_SCAN_PING,
    WIFI_SCAN_ARP,
    WIFI_SCAN_PORT,
    WIFI_SCAN_SSH,
    WIFI_SCAN_TELNET,
    // Attacks
    WIFI_ATTACK_DEAUTH_FLOOD,
    WIFI_ATTACK_DEAUTH_TARGETED,
    WIFI_ATTACK_BEACON_LIST,
    WIFI_ATTACK_BEACON_RANDOM,
    WIFI_ATTACK_RICKROLL,
    WIFI_ATTACK_PROBE_FLOOD,
    WIFI_ATTACK_AP_CLONE
};

// ── Menu Node ───────────────────────────────────────────────
struct MenuNode {
    String label;
    WiFiScanMode scanMode;
    bool hasSubmenu;
    MenuNode* parent;
    LinkedList<MenuNode*> children;

    MenuNode() : scanMode(WIFI_SCAN_OFF), hasSubmenu(false), parent(nullptr) {}
};

// ── Feature flags (set via build_flags) ─────────────────────
#ifndef HAS_SCREEN
#define HAS_SCREEN  1
#endif
#ifndef HAS_BUTTONS
#define HAS_BUTTONS 1
#endif
#ifndef HAS_SD
#define HAS_SD      1
#endif
#ifndef HAS_BT
#define HAS_BT      1
#endif
#ifndef HAS_BATTERY
#define HAS_BATTERY 1
#endif
#ifndef HAS_MQTT
#define HAS_MQTT    1
#endif

#endif
