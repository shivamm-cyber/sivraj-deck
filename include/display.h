/*=============================================================
 * Display.h — TFT rendering: status bar, menus, scan output
 *=============================================================*/
#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "configs.h"

// Color palette — industrial, no-nonsense
#define COL_BG          TFT_BLACK
#define COL_BG_NAVY     0x000A  // very dark navy
#define COL_TEXT         TFT_WHITE
#define COL_TEXT_DIM     0x7BEF  // light grey
#define COL_HEADER_BG   0x0011  // dark navy
#define COL_HIGHLIGHT   TFT_WHITE
#define COL_HIGHLIGHT_BG TFT_DARKGREY
#define COL_STATUS_OK   0x07E0  // dark green
#define COL_STATUS_WARN TFT_ORANGE
#define COL_STATUS_ERR  TFT_RED
#define COL_SEPARATOR   0x3186  // dark grey line
#define COL_SELECTED_BG 0x2945  // dark blue-grey selection
#define COL_RSSI_HIGH   0x07E0  // green
#define COL_RSSI_MED    TFT_YELLOW
#define COL_RSSI_LOW    TFT_ORANGE
#define COL_RSSI_WEAK   TFT_RED
#define COL_BAR_BG      0x18E3  // dark grey for bar backgrounds
#define COL_CURSOR      TFT_CYAN

class DisplayManager {
public:
    DisplayManager() : spr(&tft) {}
    TFT_eSPI tft;
    TFT_eSprite spr;

    bool begin();

    // Full-screen operations
    void clear();
    void pushSprite();  // push sprite buffer to TFT

    // Status bar (top 16px)
    void drawStatusBar(uint8_t channel, uint16_t apCount, uint16_t stCount,
                       uint32_t pps, uint8_t battPct, bool charging,
                       bool sdOk, bool mqttOk);

    // Menu rendering
    void drawMenuHeader(const char* title);
    void drawMenuItems(const char* items[], uint8_t count,
                       uint8_t selected, uint8_t scrollOffset);
    void drawMenuItem(uint8_t row, const char* text, bool selected);

    // Scan status screens
    void drawPacketMonitor(uint32_t pps, uint32_t totalPkts,
                           uint8_t channel, uint16_t apCount,
                           uint16_t stCount);
    void drawChannelGraph(float channels[14]);
    void drawAPList(const char* ssids[], const int8_t rssi[],
                    const uint8_t channels[], uint8_t count,
                    uint8_t selected, uint8_t scrollOffset);
    void drawStationList(const char* macs[], const int8_t rssi[],
                         uint8_t count, uint8_t selected,
                         uint8_t scrollOffset);
    void drawScanProgress(const char* title, const char* status,
                          uint16_t found, uint32_t elapsed);
    void drawAttackStatus(const char* attackName, uint32_t pktSent,
                          uint32_t elapsed, bool running);
    void drawEAPOLCapture(uint8_t handshakes, uint32_t eapolPkts);

    // Utility screens
    void drawDeviceInfo(const char* fwVer, uint32_t freeHeap,
                        uint32_t uptime, const char* mac,
                        uint16_t battMv, bool sdPresent);
    void drawTextScreen(const char* title, const char* lines[],
                        uint8_t lineCount);
    void drawConfirmDialog(const char* title, const char* msg);
    void drawProgressBar(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                         uint8_t percent, uint16_t color);
    void drawRSSIBar(uint16_t x, uint16_t y, int8_t rssi);

    // Drawing canvas (for Draw app)
    void initCanvas();
    void drawPixel(uint16_t x, uint16_t y, uint16_t color);

    // Alert overlay
    void drawAlert(const char* msg, uint16_t color = COL_STATUS_ERR);

private:
    bool _initialized = false;
    uint16_t _spriteH = 0;
    uint16_t _pushY = 0;
    uint16_t getBGColor();
    uint16_t rssiColor(int8_t rssi);
    void drawIcon8(uint16_t x, uint16_t y, const uint8_t* icon,
                   uint16_t color);
};

extern DisplayManager display;

#endif // DISPLAY_H
