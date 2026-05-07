/*=============================================================
 * Display.cpp — TFT rendering engine (sprite-buffered)
 *=============================================================*/
#include "Display.h"
#include "Assets.h"
#include "BatteryInterface.h"

DisplayManager display;

bool DisplayManager::begin() {
    #if HAS_SCREEN
    tft.init();
    tft.setRotation(3);  // landscape — proven working
    tft.fillScreen(COL_BG);

    // 8-bit sprite = 76,800 bytes (fits in non-PSRAM ESP32)
    spr.setColorDepth(8);
    void* sprPtr = spr.createSprite(TFT_SCREEN_WIDTH, TFT_SCREEN_HEIGHT);
    if (!sprPtr) {
        Serial.println(F("[DISP] Sprite FAILED"));
        _initialized = false;
        return false;
    }
    spr.fillSprite(COL_BG);

    _initialized = true;
    Serial.printf("[DISP] Init OK (8-bit sprite, heap=%lu)\n", ESP.getFreeHeap());
    return true;
    #else
    return false;
    #endif
}

void DisplayManager::clear() {
    if (!_initialized) return;
    spr.fillSprite(getBGColor());
}

void DisplayManager::pushSprite() {
    if (!_initialized) return;
    spr.pushSprite(0, 0);
}

uint16_t DisplayManager::getBGColor() {
    return COL_BG;
}

uint16_t DisplayManager::rssiColor(int8_t rssi) {
    if (rssi >= -50) return COL_RSSI_HIGH;
    if (rssi >= -65) return COL_RSSI_MED;
    if (rssi >= -80) return COL_RSSI_LOW;
    return COL_RSSI_WEAK;
}

void DisplayManager::drawIcon8(uint16_t x, uint16_t y,
                                const uint8_t* icon, uint16_t color) {
    for (int row = 0; row < 8; row++) {
        uint8_t bits = pgm_read_byte(&icon[row]);
        for (int col = 0; col < 8; col++) {
            if (bits & (0x80 >> col)) {
                spr.drawPixel(x + col, y + row, color);
            }
        }
    }
}

/* ── Status Bar ────────────────────────────────────────── */
void DisplayManager::drawStatusBar(uint8_t channel, uint16_t apCount,
                                    uint16_t stCount, uint32_t pps,
                                    uint8_t battPct, bool charging,
                                    bool sdOk, bool mqttOk) {
    // Background
    spr.fillRect(0, 0, TFT_SCREEN_WIDTH, STATUS_BAR_H, COL_BG_NAVY);

    // Channel / counts — compact for 240px
    char statusLine[40];
    snprintf(statusLine, sizeof(statusLine),
             "CH%d AP%d ST%d %lu/s",
             channel, apCount, stCount, pps);
    spr.setTextFont(1);
    spr.setTextColor(mqttOk ? COL_STATUS_OK : COL_TEXT, COL_BG_NAVY);
    spr.drawString(statusLine, 2, 4);

    // Battery on right side
    uint16_t battCol = battPct > 20 ? COL_STATUS_OK :
                       (battPct > 10 ? COL_STATUS_WARN : COL_STATUS_ERR);
    char battStr[8];
    snprintf(battStr, sizeof(battStr), "%d%%", battPct);

    // Battery bar (16x8 at right)
    uint16_t bx = TFT_SCREEN_WIDTH - 38;
    spr.drawRect(bx, 4, 16, 8, battCol);
    spr.fillRect(bx + 16, 6, 2, 4, battCol);
    uint8_t fillW = (uint8_t)((battPct / 100.0f) * 14);
    spr.fillRect(bx + 1, 5, fillW, 6, battCol);
    spr.setTextColor(battCol, COL_BG_NAVY);
    spr.drawString(battStr, TFT_SCREEN_WIDTH - 20, 4);

    // Bottom separator
    spr.drawFastHLine(0, STATUS_BAR_H - 1, TFT_SCREEN_WIDTH, COL_SEPARATOR);
}

/* ── Menu Header ───────────────────────────────────────── */
void DisplayManager::drawMenuHeader(const char* title) {
    spr.fillRect(0, STATUS_BAR_H, TFT_SCREEN_WIDTH, HEADER_H, COL_HEADER_BG);
    spr.setTextFont(2);  // smaller font for 240px
    spr.setTextColor(COL_TEXT, COL_HEADER_BG);

    uint16_t tw = spr.textWidth(title);
    uint16_t tx = (TFT_SCREEN_WIDTH - tw) / 2;
    spr.drawString(title, tx, STATUS_BAR_H + 4);

    spr.drawFastHLine(0, STATUS_BAR_H + HEADER_H - 1,
                      TFT_SCREEN_WIDTH, COL_SEPARATOR);
}

/* ── Menu Items ────────────────────────────────────────── */
void DisplayManager::drawMenuItems(const char* items[], uint8_t count,
                                    uint8_t selected, uint8_t scrollOffset) {
    uint16_t yStart = MENU_Y_START;
    uint8_t visible = min((uint8_t)MENU_ITEMS_VISIBLE,
                          (uint8_t)(count - scrollOffset));

    for (uint8_t i = 0; i < visible; i++) {
        uint8_t idx = scrollOffset + i;
        bool sel = (idx == selected);
        drawMenuItem(i, items[idx], sel);
    }

    // Scroll indicators
    if (scrollOffset > 0) {
        drawIcon8(TFT_SCREEN_WIDTH - 12, yStart - 2,
                  icon_arrow_up, COL_TEXT_DIM);
    }
    if (scrollOffset + MENU_ITEMS_VISIBLE < count) {
        drawIcon8(TFT_SCREEN_WIDTH - 12,
                  yStart + (MENU_ITEMS_VISIBLE * MENU_ITEM_H) - 8,
                  icon_arrow_down, COL_TEXT_DIM);
    }
}

void DisplayManager::drawMenuItem(uint8_t row, const char* text,
                                   bool selected) {
    uint16_t y = MENU_Y_START + (row * MENU_ITEM_H);

    if (selected) {
        spr.fillRect(0, y, TFT_SCREEN_WIDTH, MENU_ITEM_H, COL_SELECTED_BG);
        spr.setTextColor(COL_HIGHLIGHT, COL_SELECTED_BG);
        spr.setTextFont(1);
        spr.drawString(">", 4, y + 8);
        spr.drawString(text, 14, y + 8);
    } else {
        spr.fillRect(0, y, TFT_SCREEN_WIDTH, MENU_ITEM_H, getBGColor());
        spr.setTextColor(COL_TEXT_DIM, getBGColor());
        spr.setTextFont(1);
        spr.drawString(text, 14, y + 8);
    }

    // Subtle bottom border
    spr.drawFastHLine(8, y + MENU_ITEM_H - 1,
                      TFT_SCREEN_WIDTH - 16, COL_SEPARATOR);
}

/* ── Packet Monitor ────────────────────────────────────── */
void DisplayManager::drawPacketMonitor(uint32_t pps, uint32_t totalPkts,
                                        uint8_t channel, uint16_t apCount,
                                        uint16_t stCount) {
    uint16_t y = MENU_Y_START;
    spr.setTextFont(2);
    spr.setTextColor(COL_TEXT, getBGColor());

    char line[48];
    snprintf(line, sizeof(line), "Packets/sec: %lu", pps);
    spr.drawString(line, 10, y);

    snprintf(line, sizeof(line), "Total:       %lu", totalPkts);
    spr.drawString(line, 10, y + 22);

    snprintf(line, sizeof(line), "Channel:     %d", channel);
    spr.drawString(line, 10, y + 44);

    snprintf(line, sizeof(line), "APs:         %d", apCount);
    spr.drawString(line, 10, y + 66);

    snprintf(line, sizeof(line), "Stations:    %d", stCount);
    spr.drawString(line, 10, y + 88);

    // Packet rate bar
    uint16_t barY = y + 120;
    spr.fillRect(10, barY, 300, 16, COL_BAR_BG);
    uint16_t barW = min((uint16_t)300, (uint16_t)(pps / 2));
    uint16_t barCol = pps > 500 ? COL_STATUS_ERR :
                      (pps > 200 ? COL_STATUS_WARN : COL_STATUS_OK);
    spr.fillRect(10, barY, barW, 16, barCol);

    // Footer
    spr.setTextFont(1);
    spr.setTextColor(COL_TEXT_DIM, getBGColor());
    spr.drawString("HOLD UP to stop", 10, TFT_SCREEN_HEIGHT - 14);
}

/* ── Channel Graph ─────────────────────────────────────── */
void DisplayManager::drawChannelGraph(float channels[14]) {
    uint16_t yBase = TFT_SCREEN_HEIGHT - 30;
    uint16_t graphH = 140;
    uint16_t barW = 18;
    uint16_t gap = 4;
    uint16_t xStart = 12;

    // Find max for normalization
    float maxVal = 1.0f;
    for (int i = 1; i <= 13; i++) {
        if (channels[i] > maxVal) maxVal = channels[i];
    }

    spr.setTextFont(1);
    for (int ch = 1; ch <= 13; ch++) {
        uint16_t x = xStart + (ch - 1) * (barW + gap);
        float norm = channels[ch] / maxVal;
        uint16_t barH = (uint16_t)(norm * graphH);

        // Bar background
        spr.fillRect(x, yBase - graphH, barW, graphH, COL_BAR_BG);

        // Active bar
        uint16_t col = (norm > 0.8f) ? COL_STATUS_ERR :
                       (norm > 0.5f) ? COL_STATUS_WARN : COL_STATUS_OK;
        spr.fillRect(x, yBase - barH, barW, barH, col);

        // Channel number
        char chStr[4];
        snprintf(chStr, sizeof(chStr), "%d", ch);
        spr.setTextColor(COL_TEXT_DIM, getBGColor());
        spr.drawString(chStr, x + 4, yBase + 2);
    }
}

/* ── AP List ───────────────────────────────────────────── */
void DisplayManager::drawAPList(const char* ssids[], const int8_t rssi[],
                                 const uint8_t channels[], uint8_t count,
                                 uint8_t selected, uint8_t scrollOffset) {
    uint16_t y = MENU_Y_START;
    spr.setTextFont(1);

    uint8_t visible = min((uint8_t)8,
                          (uint8_t)(count - scrollOffset));

    for (uint8_t i = 0; i < visible; i++) {
        uint8_t idx = scrollOffset + i;
        uint16_t rowY = y + (i * 24);
        bool sel = (idx == selected);

        if (sel) {
            spr.fillRect(0, rowY, TFT_SCREEN_WIDTH, 23, COL_SELECTED_BG);
        }

        uint16_t textCol = sel ? COL_HIGHLIGHT : COL_TEXT;
        uint16_t bgCol = sel ? COL_SELECTED_BG : getBGColor();
        spr.setTextColor(textCol, bgCol);

        // SSID (truncated to 20 chars)
        char ssidTrunc[21];
        strncpy(ssidTrunc, ssids[idx], 20);
        ssidTrunc[20] = '\0';
        spr.drawString(ssidTrunc, 4, rowY + 4);

        // Channel
        char chStr[6];
        snprintf(chStr, sizeof(chStr), "CH%d", channels[idx]);
        spr.drawString(chStr, 180, rowY + 4);

        // RSSI value
        char rssiStr[8];
        snprintf(rssiStr, sizeof(rssiStr), "%ddBm", rssi[idx]);
        spr.setTextColor(rssiColor(rssi[idx]), bgCol);
        spr.drawString(rssiStr, 220, rowY + 4);

        // RSSI bar
        drawRSSIBar(280, rowY + 4, rssi[idx]);

        // Separator
        spr.drawFastHLine(0, rowY + 23, TFT_SCREEN_WIDTH, COL_SEPARATOR);
    }
}

/* ── Station List ──────────────────────────────────────── */
void DisplayManager::drawStationList(const char* macs[],
                                      const int8_t rssi[],
                                      uint8_t count, uint8_t selected,
                                      uint8_t scrollOffset) {
    uint16_t y = MENU_Y_START;
    spr.setTextFont(1);

    uint8_t visible = min((uint8_t)8,
                          (uint8_t)(count - scrollOffset));

    for (uint8_t i = 0; i < visible; i++) {
        uint8_t idx = scrollOffset + i;
        uint16_t rowY = y + (i * 24);
        bool sel = (idx == selected);

        if (sel) {
            spr.fillRect(0, rowY, TFT_SCREEN_WIDTH, 23, COL_SELECTED_BG);
        }

        uint16_t textCol = sel ? COL_HIGHLIGHT : COL_TEXT;
        uint16_t bgCol = sel ? COL_SELECTED_BG : getBGColor();
        spr.setTextColor(textCol, bgCol);

        spr.drawString(macs[idx], 4, rowY + 4);

        char rssiStr[8];
        snprintf(rssiStr, sizeof(rssiStr), "%ddBm", rssi[idx]);
        spr.setTextColor(rssiColor(rssi[idx]), bgCol);
        spr.drawString(rssiStr, 220, rowY + 4);

        drawRSSIBar(280, rowY + 4, rssi[idx]);
        spr.drawFastHLine(0, rowY + 23, TFT_SCREEN_WIDTH, COL_SEPARATOR);
    }
}

/* ── Scan Progress ─────────────────────────────────────── */
void DisplayManager::drawScanProgress(const char* title, const char* status,
                                       uint16_t found, uint32_t elapsed) {
    uint16_t y = MENU_Y_START + 10;
    spr.setTextFont(2);
    spr.setTextColor(COL_TEXT, getBGColor());
    spr.drawString(title, 10, y);

    spr.setTextFont(1);
    spr.setTextColor(COL_TEXT_DIM, getBGColor());
    spr.drawString(status, 10, y + 24);

    char buf[32];
    snprintf(buf, sizeof(buf), "Found: %d", found);
    spr.setTextColor(COL_STATUS_OK, getBGColor());
    spr.drawString(buf, 10, y + 44);

    snprintf(buf, sizeof(buf), "Elapsed: %lus", elapsed / 1000);
    spr.setTextColor(COL_TEXT_DIM, getBGColor());
    spr.drawString(buf, 10, y + 60);

    // Animated dots
    uint8_t dots = (millis() / 500) % 4;
    String dotStr = "";
    for (uint8_t i = 0; i < dots; i++) dotStr += ".";
    spr.drawString(dotStr, 200, y + 24);

    spr.setTextFont(1);
    spr.setTextColor(COL_TEXT_DIM, getBGColor());
    spr.drawString("HOLD UP to stop", 10, TFT_SCREEN_HEIGHT - 14);
}

/* ── Attack Status ─────────────────────────────────────── */
void DisplayManager::drawAttackStatus(const char* attackName,
                                       uint32_t pktSent,
                                       uint32_t elapsed, bool running) {
    uint16_t y = MENU_Y_START + 10;

    spr.setTextFont(2);
    spr.setTextColor(running ? COL_STATUS_ERR : COL_TEXT, getBGColor());
    spr.drawString(attackName, 10, y);

    spr.setTextColor(running ? COL_STATUS_WARN : COL_TEXT_DIM, getBGColor());
    spr.drawString(running ? "ACTIVE" : "STOPPED", 10, y + 24);

    spr.setTextFont(1);
    spr.setTextColor(COL_TEXT, getBGColor());
    char buf[48];
    snprintf(buf, sizeof(buf), "Packets sent: %lu", pktSent);
    spr.drawString(buf, 10, y + 50);

    snprintf(buf, sizeof(buf), "Duration: %lus", elapsed / 1000);
    spr.drawString(buf, 10, y + 66);

    if (running) {
        // Pulsing indicator
        uint8_t pulse = (millis() / 250) % 2;
        spr.fillCircle(300, y + 6, 5,
                       pulse ? COL_STATUS_ERR : COL_BG);
    }

    spr.setTextColor(COL_TEXT_DIM, getBGColor());
    spr.drawString("HOLD UP to stop", 10, TFT_SCREEN_HEIGHT - 14);
}

/* ── EAPOL Capture ─────────────────────────────────────── */
void DisplayManager::drawEAPOLCapture(uint8_t handshakes,
                                       uint32_t eapolPkts) {
    uint16_t y = MENU_Y_START + 10;
    spr.setTextFont(2);
    spr.setTextColor(COL_TEXT, getBGColor());
    spr.drawString("EAPOL/PMKID Capture", 10, y);

    spr.setTextFont(1);
    char buf[48];
    snprintf(buf, sizeof(buf), "Handshakes:  %d", handshakes);
    spr.setTextColor(handshakes > 0 ? COL_STATUS_OK : COL_TEXT_DIM,
                     getBGColor());
    spr.drawString(buf, 10, y + 30);

    snprintf(buf, sizeof(buf), "EAPOL pkts:  %lu", (unsigned long)eapolPkts);
    spr.setTextColor(COL_TEXT, getBGColor());
    spr.drawString(buf, 10, y + 46);

    spr.setTextColor(COL_TEXT_DIM, getBGColor());
    spr.drawString("HOLD UP to stop", 10, TFT_SCREEN_HEIGHT - 14);
}

/* ── Device Info ───────────────────────────────────────── */
void DisplayManager::drawDeviceInfo(const char* fwVer, uint32_t freeHeap,
                                     uint32_t uptime, const char* mac,
                                     uint16_t battMv, bool sdPresent) {
    uint16_t y = MENU_Y_START + 4;
    spr.setTextFont(1);
    spr.setTextColor(COL_TEXT, getBGColor());

    char buf[48];
    uint16_t lineH = 18;

    snprintf(buf, sizeof(buf), "Firmware:  %s", fwVer);
    spr.drawString(buf, 10, y); y += lineH;

    snprintf(buf, sizeof(buf), "Free Heap: %lu bytes", freeHeap);
    spr.drawString(buf, 10, y); y += lineH;

    uint32_t sec = uptime / 1000;
    snprintf(buf, sizeof(buf), "Uptime:    %02d:%02d:%02d",
             (int)(sec/3600), (int)((sec%3600)/60), (int)(sec%60));
    spr.drawString(buf, 10, y); y += lineH;

    snprintf(buf, sizeof(buf), "MAC:       %s", mac);
    spr.drawString(buf, 10, y); y += lineH;

    snprintf(buf, sizeof(buf), "Battery:   %dmV", battMv);
    spr.drawString(buf, 10, y); y += lineH;

    snprintf(buf, sizeof(buf), "SD Card:   %s", sdPresent ? "YES" : "NO");
    spr.drawString(buf, 10, y); y += lineH;

    snprintf(buf, sizeof(buf), "ESP-IDF:   %s", esp_get_idf_version());
    spr.drawString(buf, 10, y); y += lineH;

    snprintf(buf, sizeof(buf), "CPU Freq:  %dMHz", getCpuFrequencyMhz());
    spr.drawString(buf, 10, y);
}

/* ── Text Screen ───────────────────────────────────────── */
void DisplayManager::drawTextScreen(const char* title,
                                     const char* lines[],
                                     uint8_t lineCount) {
    drawMenuHeader(title);
    spr.setTextFont(1);
    spr.setTextColor(COL_TEXT, getBGColor());

    for (uint8_t i = 0; i < lineCount && i < 12; i++) {
        spr.drawString(lines[i], 10, MENU_Y_START + (i * 16));
    }
}

/* ── Confirm Dialog ────────────────────────────────────── */
void DisplayManager::drawConfirmDialog(const char* title,
                                        const char* msg) {
    // Semi-transparent overlay
    spr.fillRect(20, 60, 280, 120, COL_BG_NAVY);
    spr.drawRect(20, 60, 280, 120, COL_SEPARATOR);

    spr.setTextFont(2);
    spr.setTextColor(COL_TEXT, COL_BG_NAVY);
    uint16_t tw = spr.textWidth(title);
    spr.drawString(title, (TFT_SCREEN_WIDTH - tw) / 2, 70);

    spr.setTextFont(1);
    spr.setTextColor(COL_TEXT_DIM, COL_BG_NAVY);
    spr.drawString(msg, 30, 100);

    spr.setTextColor(COL_STATUS_OK, COL_BG_NAVY);
    spr.drawString("SELECT = Confirm", 30, 140);
    spr.setTextColor(COL_TEXT_DIM, COL_BG_NAVY);
    spr.drawString("HOLD UP = Cancel", 30, 156);
}

/* ── Progress Bar ──────────────────────────────────────── */
void DisplayManager::drawProgressBar(uint16_t x, uint16_t y,
                                      uint16_t w, uint16_t h,
                                      uint8_t percent, uint16_t color) {
    spr.drawRect(x, y, w, h, COL_SEPARATOR);
    uint16_t fillW = (uint16_t)((percent / 100.0f) * (w - 2));
    spr.fillRect(x + 1, y + 1, fillW, h - 2, color);
}

/* ── RSSI Bar ──────────────────────────────────────────── */
void DisplayManager::drawRSSIBar(uint16_t x, uint16_t y, int8_t rssi) {
    // 4-bar signal indicator
    int8_t norm = constrain(rssi, -100, -20);
    uint8_t bars = map(norm, -100, -20, 0, 4);
    uint16_t col = rssiColor(rssi);

    for (uint8_t i = 0; i < 4; i++) {
        uint16_t barH = 3 + (i * 2);
        uint16_t barY = y + (8 - barH);
        uint16_t barCol = (i < bars) ? col : COL_BAR_BG;
        spr.fillRect(x + (i * 6), barY, 4, barH, barCol);
    }
}

/* ── Canvas for Draw app ───────────────────────────────── */
void DisplayManager::initCanvas() {
    spr.fillRect(0, STATUS_BAR_H, TFT_SCREEN_WIDTH,
                 TFT_SCREEN_HEIGHT - STATUS_BAR_H, COL_BG);
}

void DisplayManager::drawPixel(uint16_t x, uint16_t y, uint16_t color) {
    if (y > STATUS_BAR_H) {
        spr.fillRect(x - 1, y - 1, 3, 3, color);
    }
}

/* ── Alert overlay ─────────────────────────────────────── */
void DisplayManager::drawAlert(const char* msg, uint16_t color) {
    spr.fillRect(10, TFT_SCREEN_HEIGHT - 40, TFT_SCREEN_WIDTH - 20,
                 30, COL_BG_NAVY);
    spr.drawRect(10, TFT_SCREEN_HEIGHT - 40, TFT_SCREEN_WIDTH - 20,
                 30, color);
    spr.setTextFont(2);
    spr.setTextColor(color, COL_BG_NAVY);
    spr.drawString(msg, 20, TFT_SCREEN_HEIGHT - 34);
}
