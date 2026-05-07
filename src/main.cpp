/*=============================================================
 * main.cpp — SIVRAJ DECK Marauder-equivalent entry point
 *=============================================================*/
#include <Arduino.h>
#include "configs.h"
#include "display.h"
#include "WiFiScan.h"
#include "MenuFunctions.h"
#include "MQTTHandler.h"
#include "EvilPortal.h"
#include "SDInterface.h"
#include "BatteryInterface.h"
#include "settings.h"
#include "Buffer.h"
#include "Switches.h"

// ── Global instances (extern'd in headers) ──────────────────
// DisplayManager  display;      — in display.cpp
// WiFiScanEngine  wifiEngine;   — in WiFiScan.cpp
// SettingsManager settingsMgr;  — in settings.cpp
// SDInterface     sdCard;       — in SDInterface.cpp
// BatteryInterface battery;     — in BatteryInterface.cpp

static MQTTHandler   mqttHandler;
static EvilPortal    evilPortal;
static MenuFunctions menu;

static uint32_t lastStatusPush = 0;
static uint32_t watchdogTimer = 0;

// ── Boot Splash ─────────────────────────────────────────────
static void showSplash() {
    display.clear();
    uint16_t cx = TFT_SCREEN_WIDTH / 2;
    display.spr.setTextDatum(MC_DATUM);

    display.spr.setTextFont(2);
    display.spr.setTextColor(TFT_CYAN, COL_BG);
    display.spr.drawString("SIVRAJ DECK", cx, TFT_SCREEN_HEIGHT / 3);

    display.spr.setTextFont(1);
    display.spr.setTextColor(COL_TEXT_DIM, COL_BG);
    display.spr.drawString("Cybersec Field Unit", cx, TFT_SCREEN_HEIGHT / 2);

    char ver[32];
    snprintf(ver, 32, "v%s", FIRMWARE_VERSION);
    display.spr.drawString(ver, cx, TFT_SCREEN_HEIGHT / 2 + 20);
    display.spr.setTextDatum(TL_DATUM);

    display.pushSprite();
    delay(1500);
}

// ── Setup ───────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println(F("\n════════════════════════════════"));
    Serial.println(F("  SIVRAJ DECK v" FIRMWARE_VERSION));
    Serial.println(F("  Marauder-class firmware"));
    Serial.println(F("════════════════════════════════\n"));

    // Display
    display.begin();
    showSplash();

    // Battery
    battery.begin();
    Serial.printf("[BAT] %d%% (%dmV)\n", battery.getPercent(), battery.getVoltageMV());

    // SD Card
    bool sdOk = sdCard.begin();
    Serial.printf("[SD] %s\n", sdOk ? "OK" : "FAILED");

    // Settings
    settingsMgr.begin();
    settingsMgr.load();

    // WiFi engine
    wifiEngine.begin();

    // MQTT
    mqttHandler.begin(MQTT_BROKER, MQTT_PORT);

    // Connect WiFi for MQTT
    display.clear();
    display.spr.setTextFont(1);
    display.spr.setTextColor(COL_TEXT, COL_BG);
    display.spr.drawString("Connecting WiFi...", 10, 20);
    display.pushSprite();

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 30) {
        delay(500); Serial.print("."); tries++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WIFI] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        mqttHandler.connect();
    } else {
        Serial.println(F("[WIFI] Offline mode"));
    }

    // Menu system
    menu.begin(&display, &wifiEngine, &evilPortal, &mqttHandler,
               &sdCard, &battery, &settingsMgr);

    watchdogTimer = millis();

    Serial.println(F("[DECK] All systems GO"));
    Serial.printf("[DECK] Free heap: %lu\n", ESP.getFreeHeap());
}

// ── Loop ────────────────────────────────────────────────────
void loop() {
    static uint32_t lastDisplayRefresh = 0;
    uint32_t now = millis();

    // Display watchdog: force refresh every 1s to prevent white screen
    if (now - lastDisplayRefresh > 1000) {
        lastDisplayRefresh = now;
        display.clear();
        display.drawStatusBar(
            wifiEngine.getChannel(),
            wifiEngine.getAPCount(),
            wifiEngine.getStationCount(),
            wifiEngine.getPacketsPerSec(),
            battery.getPercent(),
            false,
            sdCard.isReady(),
            mqttHandler.isConnected()
        );
        display.pushSprite();
    }

    // Core loop drivers
    wifiEngine.update();
    menu.update();
    evilPortal.handleClient();
    battery.update();

    // Critical battery alert
    if (battery.getVoltageMV() < CRIT_BAT_MV) {
        display.drawAlert("LOW BATTERY!", COL_STATUS_ERR);
        display.pushSprite();
    }

    // MQTT status push at interval
    if (now - lastStatusPush >= MQTT_PUBLISH_INTERVAL) {
        mqttHandler.loop();
        if (mqttHandler.isConnected()) {
            char payload[256];
            snprintf(payload, sizeof(payload),
                "{\"heap\":%lu,\"batt\":%d,\"aps\":%d,\"sts\":%d,\"pps\":%lu,\"ch\":%d,\"up\":%lu}",
                ESP.getFreeHeap(), battery.getPercent(),
                wifiEngine.getAPCount(), wifiEngine.getStationCount(),
                wifiEngine.getPacketsPerSec(), wifiEngine.getChannel(),
                now / 1000);
            mqttHandler.publish(MQTT_TOPIC_STATUS, payload);
        }
        lastStatusPush = now;
    }

    yield();
}

