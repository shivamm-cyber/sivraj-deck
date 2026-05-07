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

// ── Boot Splash ─────────────────────────────────────────────
static void showSplash() {
    display.clear();
    display.spr.setTextFont(4);
    display.spr.setTextColor(TFT_CYAN, COL_BG);
    display.spr.setTextDatum(MC_DATUM);
    display.spr.drawString("SIVRAJ DECK", 160, 60);

    display.spr.setTextFont(2);
    display.spr.setTextColor(COL_TEXT_DIM, COL_BG);
    display.spr.drawString("Cybersecurity Field Unit", 160, 100);

    display.spr.setTextFont(1);
    char ver[32];
    snprintf(ver, 32, "v%s", FIRMWARE_VERSION);
    display.spr.drawString(ver, 160, 130);
    display.spr.setTextDatum(TL_DATUM);

    // Progress bar
    for (int i = 0; i <= 280; i += 8) {
        display.spr.fillRect(20, 160, 280, 6, COL_BAR_BG);
        display.spr.fillRect(20, 160, i, 6, TFT_CYAN);
        display.pushSprite();
        delay(10);
    }
    delay(1000);
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
    display.spr.setTextFont(2);
    display.spr.setTextColor(COL_TEXT, COL_BG);
    display.spr.drawString("Connecting WiFi...", 10, 40);
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

    Serial.println(F("[DECK] All systems GO"));
    Serial.printf("[DECK] Free heap: %lu\n", ESP.getFreeHeap());
}

// ── Loop ────────────────────────────────────────────────────
void loop() {
    uint32_t now = millis();

    wifiEngine.update();
    menu.update();
    evilPortal.handleClient();
    battery.update();

    if (battery.getVoltageMV() < CRIT_BAT_MV) {
        display.drawAlert("LOW BATTERY!", COL_STATUS_ERR);
        display.pushSprite();
    }

    // MQTT status push
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
