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

    _initialized = true;
    Serial.printf("[DISP] Init OK (direct render, heap=%lu)\n", ESP.getFreeHeap());
    return true;
    #else
    return false;
    #endif
}

void DisplayManager::clear() {
    if (!_initialized) return;
    tft.fillScreen(getBGColor());
}

void DisplayManager::pushSprite() {
    // No-op in direct render mode — each draw call goes straight to TFT
    if (!_initialized) return;
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
                tft.drawPixel(x + col, y + row, color);
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
    tft.fillRect(0, 0, TFT_SCREEN_WIDTH, STATUS_BAR_H, COL_BG_NAVY);

    // Channel / counts — compact for 240px
    char statusLine[40];
    snprintf(statusLine, sizeof(statusLine),
             "CH%d AP%d ST%d %lu/s",
             channel, apCount, stCount, pps);
    tft.setTextFont(1);
    tft.setTextColor(mqttOk ? COL_STATUS_OK : COL_TEXT, COL_BG_NAVY);
    tft.drawString(statusLine, 2, 4);

    // Battery on right side
    uint16_t battCol = battPct > 20 ? COL_STATUS_OK :
                       (battPct > 10 ? COL_STATUS_WARN : COL_STATUS_ERR);
    char battStr[8];
    snprintf(battStr, sizeof(battStr), "%d%%", battPct);

    // Battery bar (16x8 at right)
    uint16_t bx = TFT_SCREEN_WIDTH - 38;
    tft.drawRect(bx, 4, 16, 8, battCol);
    tft.fillRect(bx + 16, 6, 2, 4, battCol);
    uint8_t fillW = (uint8_t)((battPct / 100.0f) * 14);
    tft.fillRect(bx + 1, 5, fillW, 6, battCol);
    tft.setTextColor(battCol, COL_BG_NAVY);
    tft.drawString(battStr, TFT_SCREEN_WIDTH - 20, 4);

    // Bottom separator
    tft.drawFastHLine(0, STATUS_BAR_H - 1, TFT_SCREEN_WIDTH, COL_SEPARATOR);
}

/* ── Menu Header ───────────────────────────────────────── */
void DisplayManager::drawMenuHeader(const char* title) {
    tft.fillRect(0, STATUS_BAR_H, TFT_SCREEN_WIDTH, HEADER_H, COL_HEADER_BG);
    tft.setTextFont(2);  // smaller font for 240px
    tft.setTextColor(COL_TEXT, COL_HEADER_BG);

    uint16_t tw = tft.textWidth(title);
    uint16_t tx = (TFT_SCREEN_WIDTH - tw) / 2;
    tft.drawString(title, tx, STATUS_BAR_H + 4);

    tft.drawFastHLine(0, STATUS_BAR_H + HEADER_H - 1,
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
        tft.fillRect(0, y, TFT_SCREEN_WIDTH, MENU_ITEM_H, COL_SELECTED_BG);
        tft.setTextColor(COL_HIGHLIGHT, COL_SELECTED_BG);
        tft.setTextFont(1);
        tft.drawString(">", 4, y + 8);
        tft.drawString(text, 14, y + 8);
    } else {
        tft.fillRect(0, y, TFT_SCREEN_WIDTH, MENU_ITEM_H, getBGColor());
        tft.setTextColor(COL_TEXT_DIM, getBGColor());
        tft.setTextFont(1);
        tft.drawString(text, 14, y + 8);
    }

    // Subtle bottom border
    tft.drawFastHLine(8, y + MENU_ITEM_H - 1,
                      TFT_SCREEN_WIDTH - 16, COL_SEPARATOR);
}

/* ── Packet Monitor ────────────────────────────────────── */
void DisplayManager::drawPacketMonitor(uint32_t pps, uint32_t totalPkts,
                                        uint8_t channel, uint16_t apCount,
                                        uint16_t stCount) {
    uint16_t y = MENU_Y_START;
    tft.setTextFont(2);
    tft.setTextColor(COL_TEXT, getBGColor());

    char line[48];
    snprintf(line, sizeof(line), "Packets/sec: %lu", pps);
    tft.drawString(line, 10, y);

    snprintf(line, sizeof(line), "Total:       %lu", totalPkts);
    tft.drawString(line, 10, y + 22);

    snprintf(line, sizeof(line), "Channel:     %d", channel);
    tft.drawString(line, 10, y + 44);

    snprintf(line, sizeof(line), "APs:         %d", apCount);
    tft.drawString(line, 10, y + 66);

    snprintf(line, sizeof(line), "Stations:    %d", stCount);
    tft.drawString(line, 10, y + 88);

    // Packet rate bar
    uint16_t barY = y + 120;
    tft.fillRect(10, barY, 300, 16, COL_BAR_BG);
    uint16_t barW = min((uint16_t)300, (uint16_t)(pps / 2));
    uint16_t barCol = pps > 500 ? COL_STATUS_ERR :
                      (pps > 200 ? COL_STATUS_WARN : COL_STATUS_OK);
    tft.fillRect(10, barY, barW, 16, barCol);

    // Footer
    tft.setTextFont(1);
    tft.setTextColor(COL_TEXT_DIM, getBGColor());
    tft.drawString("HOLD UP to stop", 10, TFT_SCREEN_HEIGHT - 14);
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

    tft.setTextFont(1);
    for (int ch = 1; ch <= 13; ch++) {
        uint16_t x = xStart + (ch - 1) * (barW + gap);
        float norm = channels[ch] / maxVal;
        uint16_t barH = (uint16_t)(norm * graphH);

        // Bar background
        tft.fillRect(x, yBase - graphH, barW, graphH, COL_BAR_BG);

        // Active bar
        uint16_t col = (norm > 0.8f) ? COL_STATUS_ERR :
                       (norm > 0.5f) ? COL_STATUS_WARN : COL_STATUS_OK;
        tft.fillRect(x, yBase - barH, barW, barH, col);

        // Channel number
        char chStr[4];
        snprintf(chStr, sizeof(chStr), "%d", ch);
        tft.setTextColor(COL_TEXT_DIM, getBGColor());
        tft.drawString(chStr, x + 4, yBase + 2);
    }
}

/* ── AP List ───────────────────────────────────────────── */
void DisplayManager::drawAPList(const char* ssids[], const int8_t rssi[],
                                 const uint8_t channels[], uint8_t count,
                                 uint8_t selected, uint8_t scrollOffset) {
    uint16_t y = MENU_Y_START;
    tft.setTextFont(1);

    uint8_t visible = min((uint8_t)8,
                          (uint8_t)(count - scrollOffset));

    for (uint8_t i = 0; i < visible; i++) {
        uint8_t idx = scrollOffset + i;
        uint16_t rowY = y + (i * 24);
        bool sel = (idx == selected);

        if (sel) {
            tft.fillRect(0, rowY, TFT_SCREEN_WIDTH, 23, COL_SELECTED_BG);
        }

        uint16_t textCol = sel ? COL_HIGHLIGHT : COL_TEXT;
        uint16_t bgCol = sel ? COL_SELECTED_BG : getBGColor();
        tft.setTextColor(textCol, bgCol);

        // SSID (truncated to 20 chars)
        char ssidTrunc[21];
        strncpy(ssidTrunc, ssids[idx], 20);
        ssidTrunc[20] = '\0';
        tft.drawString(ssidTrunc, 4, rowY + 4);

        // Channel
        char chStr[6];
        snprintf(chStr, sizeof(chStr), "CH%d", channels[idx]);
        tft.drawString(chStr, 180, rowY + 4);

        // RSSI value
        char rssiStr[8];
        snprintf(rssiStr, sizeof(rssiStr), "%ddBm", rssi[idx]);
        tft.setTextColor(rssiColor(rssi[idx]), bgCol);
        tft.drawString(rssiStr, 220, rowY + 4);

        // RSSI bar
        drawRSSIBar(280, rowY + 4, rssi[idx]);

        // Separator
        tft.drawFastHLine(0, rowY + 23, TFT_SCREEN_WIDTH, COL_SEPARATOR);
    }
}

/* ── Station List ──────────────────────────────────────── */
void DisplayManager::drawStationList(const char* macs[],
                                      const int8_t rssi[],
                                      uint8_t count, uint8_t selected,
                                      uint8_t scrollOffset) {
    uint16_t y = MENU_Y_START;
    tft.setTextFont(1);

    uint8_t visible = min((uint8_t)8,
                          (uint8_t)(count - scrollOffset));

    for (uint8_t i = 0; i < visible; i++) {
        uint8_t idx = scrollOffset + i;
        uint16_t rowY = y + (i * 24);
        bool sel = (idx == selected);

        if (sel) {
            tft.fillRect(0, rowY, TFT_SCREEN_WIDTH, 23, COL_SELECTED_BG);
        }

        uint16_t textCol = sel ? COL_HIGHLIGHT : COL_TEXT;
        uint16_t bgCol = sel ? COL_SELECTED_BG : getBGColor();
        tft.setTextColor(textCol, bgCol);

        tft.drawString(macs[idx], 4, rowY + 4);

        char rssiStr[8];
        snprintf(rssiStr, sizeof(rssiStr), "%ddBm", rssi[idx]);
        tft.setTextColor(rssiColor(rssi[idx]), bgCol);
        tft.drawString(rssiStr, 220, rowY + 4);

        drawRSSIBar(280, rowY + 4, rssi[idx]);
        tft.drawFastHLine(0, rowY + 23, TFT_SCREEN_WIDTH, COL_SEPARATOR);
    }
}

/* ── Scan Progress ─────────────────────────────────────── */
void DisplayManager::drawScanProgress(const char* title, const char* status,
                                       uint16_t found, uint32_t elapsed) {
    uint16_t y = MENU_Y_START + 10;
    tft.setTextFont(2);
    tft.setTextColor(COL_TEXT, getBGColor());
    tft.drawString(title, 10, y);

    tft.setTextFont(1);
    tft.setTextColor(COL_TEXT_DIM, getBGColor());
    tft.drawString(status, 10, y + 24);

    char buf[32];
    snprintf(buf, sizeof(buf), "Found: %d", found);
    tft.setTextColor(COL_STATUS_OK, getBGColor());
    tft.drawString(buf, 10, y + 44);

    snprintf(buf, sizeof(buf), "Elapsed: %lus", elapsed / 1000);
    tft.setTextColor(COL_TEXT_DIM, getBGColor());
    tft.drawString(buf, 10, y + 60);

    // Animated dots
    uint8_t dots = (millis() / 500) % 4;
    String dotStr = "";
    for (uint8_t i = 0; i < dots; i++) dotStr += ".";
    tft.drawString(dotStr, 200, y + 24);

    tft.setTextFont(1);
    tft.setTextColor(COL_TEXT_DIM, getBGColor());
    tft.drawString("HOLD UP to stop", 10, TFT_SCREEN_HEIGHT - 14);
}

/* ── Attack Status ─────────────────────────────────────── */
void DisplayManager::drawAttackStatus(const char* attackName,
                                       uint32_t pktSent,
                                       uint32_t elapsed, bool running) {
    uint16_t y = MENU_Y_START + 10;

    tft.setTextFont(2);
    tft.setTextColor(running ? COL_STATUS_ERR : COL_TEXT, getBGColor());
    tft.drawString(attackName, 10, y);

    tft.setTextColor(running ? COL_STATUS_WARN : COL_TEXT_DIM, getBGColor());
    tft.drawString(running ? "ACTIVE" : "STOPPED", 10, y + 24);

    tft.setTextFont(1);
    tft.setTextColor(COL_TEXT, getBGColor());
    char buf[48];
    snprintf(buf, sizeof(buf), "Packets sent: %lu", pktSent);
    tft.drawString(buf, 10, y + 50);

    snprintf(buf, sizeof(buf), "Duration: %lus", elapsed / 1000);
    tft.drawString(buf, 10, y + 66);

    if (running) {
        // Pulsing indicator
        uint8_t pulse = (millis() / 250) % 2;
        tft.fillCircle(300, y + 6, 5,
                       pulse ? COL_STATUS_ERR : COL_BG);
    }

    tft.setTextColor(COL_TEXT_DIM, getBGColor());
    tft.drawString("HOLD UP to stop", 10, TFT_SCREEN_HEIGHT - 14);
}

/* ── EAPOL Capture ─────────────────────────────────────── */
void DisplayManager::drawEAPOLCapture(uint8_t handshakes,
                                       uint32_t eapolPkts) {
    uint16_t y = MENU_Y_START + 10;
    tft.setTextFont(2);
    tft.setTextColor(COL_TEXT, getBGColor());
    tft.drawString("EAPOL/PMKID Capture", 10, y);

    tft.setTextFont(1);
    char buf[48];
    snprintf(buf, sizeof(buf), "Handshakes:  %d", handshakes);
    tft.setTextColor(handshakes > 0 ? COL_STATUS_OK : COL_TEXT_DIM,
                     getBGColor());
    tft.drawString(buf, 10, y + 30);

    snprintf(buf, sizeof(buf), "EAPOL pkts:  %lu", (unsigned long)eapolPkts);
    tft.setTextColor(COL_TEXT, getBGColor());
    tft.drawString(buf, 10, y + 46);

    tft.setTextColor(COL_TEXT_DIM, getBGColor());
    tft.drawString("HOLD UP to stop", 10, TFT_SCREEN_HEIGHT - 14);
}

/* ── Device Info ───────────────────────────────────────── */
void DisplayManager::drawDeviceInfo(const char* fwVer, uint32_t freeHeap,
                                     uint32_t uptime, const char* mac,
                                     uint16_t battMv, bool sdPresent) {
    uint16_t y = MENU_Y_START + 4;
    tft.setTextFont(1);
    tft.setTextColor(COL_TEXT, getBGColor());

    char buf[48];
    uint16_t lineH = 18;

    snprintf(buf, sizeof(buf), "Firmware:  %s", fwVer);
    tft.drawString(buf, 10, y); y += lineH;

    snprintf(buf, sizeof(buf), "Free Heap: %lu bytes", freeHeap);
    tft.drawString(buf, 10, y); y += lineH;

    uint32_t sec = uptime / 1000;
    snprintf(buf, sizeof(buf), "Uptime:    %02d:%02d:%02d",
             (int)(sec/3600), (int)((sec%3600)/60), (int)(sec%60));
    tft.drawString(buf, 10, y); y += lineH;

    snprintf(buf, sizeof(buf), "MAC:       %s", mac);
    tft.drawString(buf, 10, y); y += lineH;

    snprintf(buf, sizeof(buf), "Battery:   %dmV", battMv);
    tft.drawString(buf, 10, y); y += lineH;

    snprintf(buf, sizeof(buf), "SD Card:   %s", sdPresent ? "YES" : "NO");
    tft.drawString(buf, 10, y); y += lineH;

    snprintf(buf, sizeof(buf), "ESP-IDF:   %s", esp_get_idf_version());
    tft.drawString(buf, 10, y); y += lineH;

    snprintf(buf, sizeof(buf), "CPU Freq:  %dMHz", getCpuFrequencyMhz());
    tft.drawString(buf, 10, y);
}

/* ── Text Screen ───────────────────────────────────────── */
void DisplayManager::drawTextScreen(const char* title,
                                     const char* lines[],
                                     uint8_t lineCount) {
    drawMenuHeader(title);
    tft.setTextFont(1);
    tft.setTextColor(COL_TEXT, getBGColor());

    for (uint8_t i = 0; i < lineCount && i < 12; i++) {
        tft.drawString(lines[i], 10, MENU_Y_START + (i * 16));
    }
}

/* ── Confirm Dialog ────────────────────────────────────── */
void DisplayManager::drawConfirmDialog(const char* title,
                                        const char* msg) {
    // Semi-transparent overlay
    tft.fillRect(20, 60, 280, 120, COL_BG_NAVY);
    tft.drawRect(20, 60, 280, 120, COL_SEPARATOR);

    tft.setTextFont(2);
    tft.setTextColor(COL_TEXT, COL_BG_NAVY);
    uint16_t tw = tft.textWidth(title);
    tft.drawString(title, (TFT_SCREEN_WIDTH - tw) / 2, 70);

    tft.setTextFont(1);
    tft.setTextColor(COL_TEXT_DIM, COL_BG_NAVY);
    tft.drawString(msg, 30, 100);

    tft.setTextColor(COL_STATUS_OK, COL_BG_NAVY);
    tft.drawString("SELECT = Confirm", 30, 140);
    tft.setTextColor(COL_TEXT_DIM, COL_BG_NAVY);
    tft.drawString("HOLD UP = Cancel", 30, 156);
}

/* ── Progress Bar ──────────────────────────────────────── */
void DisplayManager::drawProgressBar(uint16_t x, uint16_t y,
                                      uint16_t w, uint16_t h,
                                      uint8_t percent, uint16_t color) {
    tft.drawRect(x, y, w, h, COL_SEPARATOR);
    uint16_t fillW = (uint16_t)((percent / 100.0f) * (w - 2));
    tft.fillRect(x + 1, y + 1, fillW, h - 2, color);
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
        tft.fillRect(x + (i * 6), barY, 4, barH, barCol);
    }
}

/* ── Canvas for Draw app ───────────────────────────────── */
void DisplayManager::initCanvas() {
    tft.fillRect(0, STATUS_BAR_H, TFT_SCREEN_WIDTH,
                 TFT_SCREEN_HEIGHT - STATUS_BAR_H, COL_BG);
}

void DisplayManager::drawPixel(uint16_t x, uint16_t y, uint16_t color) {
    if (y > STATUS_BAR_H) {
        tft.fillRect(x - 1, y - 1, 3, 3, color);
    }
}

/* ── Alert overlay ─────────────────────────────────────── */
void DisplayManager::drawAlert(const char* msg, uint16_t color) {
    tft.fillRect(10, TFT_SCREEN_HEIGHT - 40, TFT_SCREEN_WIDTH - 20,
                 30, COL_BG_NAVY);
    tft.drawRect(10, TFT_SCREEN_HEIGHT - 40, TFT_SCREEN_WIDTH - 20,
                 30, color);
    tft.setTextFont(2);
    tft.setTextColor(color, COL_BG_NAVY);
    tft.drawString(msg, 20, TFT_SCREEN_HEIGHT - 34);
}
