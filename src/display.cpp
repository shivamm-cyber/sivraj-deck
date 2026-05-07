/*=============================================================
 * Display.cpp — TFT rendering engine (sprite-buffered)
 *=============================================================*/
#include "Display.h"
#include "Assets.h"
#include "BatteryInterface.h"

DisplayManager display;

bool DisplayManager::begin() {
  Serial.println(F("[DISP] Initializing TFT..."));
#if HAS_SCREEN
  tft.init();
  tft.setRotation(0);  // portrait
  tft.fillScreen(TFT_BLACK);

  // 16-bit sprite with half-height fallback
  spr.setColorDepth(16);
  void *sprPtr = spr.createSprite(TFT_SCREEN_WIDTH, TFT_SCREEN_HEIGHT);
  if (!sprPtr) {
    Serial.println(F("[DISP] Full sprite fail -> half"));
    sprPtr = spr.createSprite(TFT_SCREEN_WIDTH, TFT_SCREEN_HEIGHT / 2);
    _spriteH = TFT_SCREEN_HEIGHT / 2;
  } else {
    _spriteH = TFT_SCREEN_HEIGHT;
  }
  if (!sprPtr) {
    Serial.println(F("[DISP] CRITICAL: No sprite!"));
    _initialized = false;
    return false;
  }
  spr.fillSprite(TFT_BLACK);
  _initialized = true;
  Serial.printf("[DISP] OK: %dx%d sprite (heap=%lu)\n",
                spr.width(), _spriteH, ESP.getFreeHeap());
  return true;
#else
  return false;
#endif
}

void DisplayManager::clear() {
  if (!_initialized) return;
  spr.fillSprite(TFT_BLACK);
}

void DisplayManager::pushSprite() {
  if (!_initialized) return;
  if (_spriteH >= TFT_SCREEN_HEIGHT) {
    spr.pushSprite(0, 0);
  } else {
    spr.pushSprite(0, 0);
    spr.pushSprite(0, _spriteH);
  }
}

uint16_t DisplayManager::getBGColor() { return COL_BG; }

uint16_t DisplayManager::rssiColor(int8_t rssi) {
  if (rssi >= -50) return COL_RSSI_HIGH;
  if (rssi >= -65) return COL_RSSI_MED;
  if (rssi >= -80) return COL_RSSI_LOW;
  return COL_RSSI_WEAK;
}

void DisplayManager::drawIcon8(uint16_t x, uint16_t y, const uint8_t *icon,
                               uint16_t color) {
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
                                   uint8_t battPct, bool charging, bool sdOk,
                                   bool mqttOk) {
  spr.fillRect(0, 0, TFT_SCREEN_WIDTH, STATUS_BAR_H, COL_HEADER_BG);
  spr.drawFastHLine(0, STATUS_BAR_H - 1, TFT_SCREEN_WIDTH, COL_SEPARATOR);

  spr.setTextFont(1);
  int16_t x = 2;

  char buf[24];
  spr.setTextColor(COL_TEXT_DIM, COL_HEADER_BG);
  snprintf(buf, sizeof(buf), "CH%02d", channel);
  spr.drawString(buf, x, 2); x += 28;

  snprintf(buf, sizeof(buf), "%dAP", apCount);
  spr.setTextColor(apCount > 0 ? COL_TEXT : COL_TEXT_DIM, COL_HEADER_BG);
  spr.drawString(buf, x, 2); x += 24;

  snprintf(buf, sizeof(buf), "%dST", stCount);
  spr.setTextColor(stCount > 0 ? COL_STATUS_WARN : COL_TEXT_DIM, COL_HEADER_BG);
  spr.drawString(buf, x, 2); x += 24;

  snprintf(buf, sizeof(buf), "%lu/s", (unsigned long)pps);
  spr.setTextColor(pps > 0 ? COL_STATUS_ERR : COL_TEXT_DIM, COL_HEADER_BG);
  spr.drawString(buf, x, 2); x += 42;

  spr.setTextColor(sdOk ? COL_STATUS_OK : COL_TEXT_DIM, COL_HEADER_BG);
  spr.drawString(sdOk ? "SD" : "  ", x, 2); x += 16;

  spr.setTextColor(mqttOk ? COL_STATUS_OK : COL_TEXT_DIM, COL_HEADER_BG);
  spr.drawString(mqttOk ? "MQ" : "  ", x, 2); x += 16;

  snprintf(buf, sizeof(buf), "%s%d%%", charging ? "+" : "", battPct);
  spr.setTextColor(battPct > 20 ? COL_TEXT_DIM : COL_STATUS_ERR, COL_HEADER_BG);
  spr.drawString(buf, TFT_SCREEN_WIDTH - 36, 2);
}

/* ── Menu Header ───────────────────────────────────────── */
void DisplayManager::drawMenuHeader(const char *title) {
  spr.fillRect(0, STATUS_BAR_H, TFT_SCREEN_WIDTH, HEADER_H, COL_HEADER_BG);
  spr.drawFastHLine(0, STATUS_BAR_H + HEADER_H - 1, TFT_SCREEN_WIDTH,
                    COL_SEPARATOR);
  spr.setTextFont(2);
  spr.setTextColor(COL_HIGHLIGHT, COL_HEADER_BG);
  spr.drawString(title, 6, STATUS_BAR_H + 2);
}

/* ── Menu Items ────────────────────────────────────────── */
void DisplayManager::drawMenuItems(const char *items[], uint8_t count,
                                   uint8_t selected, uint8_t scrollOffset) {
  for (uint8_t i = 0; i < MENU_ITEMS_VISIBLE && i < count; i++) {
    uint8_t idx = i + scrollOffset;
    if (idx >= count) break;
    drawMenuItem(i, items[idx], idx == selected);
  }
}

void DisplayManager::drawMenuItem(uint8_t row, const char *text,
                                  bool selected) {
  uint16_t y = MENU_Y_START + (row * MENU_ITEM_H);
  uint16_t bg = selected ? COL_SELECTED_BG : getBGColor();

  spr.fillRect(0, y, TFT_SCREEN_WIDTH, MENU_ITEM_H, bg);
  if (selected) {
    spr.setTextColor(COL_CURSOR, bg);
    spr.drawString(">", 4, y + 2);
  }
  spr.setTextFont(1);
  spr.setTextColor(selected ? COL_HIGHLIGHT : COL_TEXT, bg);
  spr.drawString(text, selected ? 16 : 8, y + 2);
  spr.drawFastHLine(0, y + MENU_ITEM_H - 1, TFT_SCREEN_WIDTH, COL_SEPARATOR);
}

/* ── Packet Monitor ──────────────────────────────────── */
void DisplayManager::drawPacketMonitor(uint32_t pps, uint32_t totalPkts,
                                       uint8_t channel, uint16_t apCount,
                                       uint16_t stCount) {
  uint16_t y = MENU_Y_START + 10;
  spr.setTextFont(2);
  spr.setTextColor(COL_TEXT, getBGColor());
  spr.drawString("PACKET MONITOR", 10, y); y += 30;

  spr.setTextFont(1);
  char buf[48];
  snprintf(buf, sizeof(buf), "PPS: %lu", (unsigned long)pps);
  spr.setTextColor(COL_STATUS_ERR, getBGColor());
  spr.drawString(buf, 10, y); y += 20;

  snprintf(buf, sizeof(buf), "Total: %lu", (unsigned long)totalPkts);
  spr.setTextColor(COL_TEXT, getBGColor());
  spr.drawString(buf, 10, y); y += 20;

  snprintf(buf, sizeof(buf), "CH:%d APs:%d STAs:%d", channel, apCount, stCount);
  spr.setTextColor(COL_TEXT_DIM, getBGColor());
  spr.drawString(buf, 10, y);

  spr.setTextColor(COL_TEXT_DIM, getBGColor());
  spr.drawString("HOLD UP to return", 10, TFT_SCREEN_HEIGHT - 14);
}

/* ── Channel Graph ───────────────────────────────────── */
void DisplayManager::drawChannelGraph(float channels[14]) {
  uint16_t y = MENU_Y_START + 10;
  spr.setTextFont(2);
  spr.setTextColor(COL_TEXT, getBGColor());
  spr.drawString("CHANNEL ANALYZER", 10, y); y += 24;

  spr.setTextFont(1);
  float maxVal = 1;
  for (uint8_t i = 0; i < 14; i++)
    if (channels[i] > maxVal) maxVal = channels[i];

  uint16_t barW = (TFT_SCREEN_WIDTH - 40) / 14;
  uint16_t barMaxH = 120;

  for (uint8_t i = 1; i <= 13; i++) {
    uint16_t barH = (channels[i - 1] / maxVal) * barMaxH;
    uint16_t x = 14 + (i - 1) * (barW + 2);
    uint16_t barY = y + barMaxH - barH;

    spr.fillRect(x, barY, barW, barH,
                 channels[i - 1] > 0 ? COL_CURSOR : COL_BAR_BG);
    spr.drawRect(x, barY, barW, barH, COL_SEPARATOR);

    char chLabel[4];
    snprintf(chLabel, sizeof(chLabel), "%d", i);
    spr.setTextColor(COL_TEXT_DIM, getBGColor());
    spr.drawString(chLabel, x + 1, y + barMaxH + 2);
  }
}

/* ── AP List ───────────────────────────────────────────── */
void DisplayManager::drawAPList(const char* ssids[], const int8_t rssi[],
                                const uint8_t channels[], uint8_t count,
                                uint8_t selected, uint8_t scrollOffset) {
  uint16_t y = MENU_Y_START;
  spr.setTextFont(1);
  uint8_t visible = min((uint8_t)MENU_ITEMS_VISIBLE, (uint8_t)(count - scrollOffset));

  for (uint8_t i = 0; i < visible; i++) {
    uint8_t idx = scrollOffset + i;
    uint16_t rowY = y + (i * MENU_ITEM_H);
    bool sel = (idx == selected);
    uint16_t bg = sel ? COL_SELECTED_BG : getBGColor();

    spr.fillRect(0, rowY, TFT_SCREEN_WIDTH, MENU_ITEM_H, bg);
    spr.setTextColor(sel ? COL_HIGHLIGHT : COL_TEXT, bg);

    char ssidTrunc[18];
    strncpy(ssidTrunc, ssids[idx], 17);
    ssidTrunc[17] = '\0';
    spr.drawString(ssidTrunc, 4, rowY + 2);

    char info[16];
    snprintf(info, sizeof(info), "%ddB CH%d", rssi[idx], channels[idx]);
    spr.setTextColor(rssiColor(rssi[idx]), bg);
    spr.drawString(info, TFT_SCREEN_WIDTH - 80, rowY + 2);

    spr.drawFastHLine(0, rowY + MENU_ITEM_H - 1, TFT_SCREEN_WIDTH, COL_SEPARATOR);
  }
}

/* ── Station List ──────────────────────────────────────── */
void DisplayManager::drawStationList(const char* macs[], const int8_t rssi[],
                                     uint8_t count, uint8_t selected,
                                     uint8_t scrollOffset) {
  uint16_t y = MENU_Y_START;
  spr.setTextFont(1);
  uint8_t visible = min((uint8_t)MENU_ITEMS_VISIBLE, (uint8_t)(count - scrollOffset));

  for (uint8_t i = 0; i < visible; i++) {
    uint8_t idx = scrollOffset + i;
    uint16_t rowY = y + (i * MENU_ITEM_H);
    bool sel = (idx == selected);
    uint16_t bg = sel ? COL_SELECTED_BG : getBGColor();

    spr.fillRect(0, rowY, TFT_SCREEN_WIDTH, MENU_ITEM_H, bg);
    spr.setTextColor(sel ? COL_HIGHLIGHT : COL_TEXT, bg);
    spr.drawString(macs[idx], 4, rowY + 2);

    char rssiStr[8];
    snprintf(rssiStr, sizeof(rssiStr), "%ddB", rssi[idx]);
    spr.setTextColor(rssiColor(rssi[idx]), bg);
    spr.drawString(rssiStr, TFT_SCREEN_WIDTH - 40, rowY + 2);

    spr.drawFastHLine(0, rowY + MENU_ITEM_H - 1, TFT_SCREEN_WIDTH, COL_SEPARATOR);
  }
}

/* ── Scan Progress ─────────────────────────────────────── */
void DisplayManager::drawScanProgress(const char* title, const char* status,
                                      uint16_t found, uint32_t elapsed) {
  uint16_t y = MENU_Y_START + 10;
  spr.setTextFont(2);
  spr.setTextColor(COL_TEXT, getBGColor());
  spr.drawString(title, 10, y); y += 24;

  spr.setTextFont(1);
  spr.setTextColor(COL_TEXT_DIM, getBGColor());
  spr.drawString(status, 10, y); y += 16;

  char buf[32];
  snprintf(buf, sizeof(buf), "Found: %d", found);
  spr.setTextColor(COL_STATUS_OK, getBGColor());
  spr.drawString(buf, 10, y); y += 16;

  snprintf(buf, sizeof(buf), "Elapsed: %lus", elapsed / 1000);
  spr.setTextColor(COL_TEXT_DIM, getBGColor());
  spr.drawString(buf, 10, y);

  spr.drawString("HOLD UP to stop", 10, TFT_SCREEN_HEIGHT - 14);
}

/* ── Attack Status ─────────────────────────────────────── */
void DisplayManager::drawAttackStatus(const char* attackName,
                                      uint32_t pktSent,
                                      uint32_t elapsed, bool running) {
  uint16_t y = MENU_Y_START + 10;
  spr.setTextFont(2);
  spr.setTextColor(running ? COL_STATUS_ERR : COL_TEXT, getBGColor());
  spr.drawString(attackName, 10, y); y += 24;

  spr.setTextColor(running ? COL_STATUS_WARN : COL_TEXT_DIM, getBGColor());
  spr.drawString(running ? "ACTIVE" : "STOPPED", 10, y); y += 20;

  spr.setTextFont(1);
  char buf[48];
  snprintf(buf, sizeof(buf), "Packets: %lu", pktSent);
  spr.setTextColor(COL_TEXT, getBGColor());
  spr.drawString(buf, 10, y); y += 16;

  snprintf(buf, sizeof(buf), "Duration: %lus", elapsed / 1000);
  spr.drawString(buf, 10, y);

  spr.setTextColor(COL_TEXT_DIM, getBGColor());
  spr.drawString("HOLD UP to stop", 10, TFT_SCREEN_HEIGHT - 14);
}

/* ── EAPOL Capture ─────────────────────────────────────── */
void DisplayManager::drawEAPOLCapture(uint8_t handshakes, uint32_t eapolPkts) {
  uint16_t y = MENU_Y_START + 10;
  spr.setTextFont(2);
  spr.setTextColor(COL_TEXT, getBGColor());
  spr.drawString("EAPOL CAPTURE", 10, y); y += 28;

  spr.setTextFont(1);
  char buf[48];
  snprintf(buf, sizeof(buf), "Handshakes: %d", handshakes);
  spr.setTextColor(handshakes > 0 ? COL_STATUS_OK : COL_TEXT_DIM, getBGColor());
  spr.drawString(buf, 10, y); y += 16;

  snprintf(buf, sizeof(buf), "EAPOL pkts: %lu", (unsigned long)eapolPkts);
  spr.setTextColor(COL_TEXT, getBGColor());
  spr.drawString(buf, 10, y);

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
  uint16_t lh = 14;

  snprintf(buf, sizeof(buf), "FW:   %s", fwVer);
  spr.drawString(buf, 6, y); y += lh;
  snprintf(buf, sizeof(buf), "Heap: %lu", freeHeap);
  spr.drawString(buf, 6, y); y += lh;
  uint32_t s = uptime / 1000;
  snprintf(buf, sizeof(buf), "Up:   %02d:%02d:%02d", (int)(s/3600), (int)((s%3600)/60), (int)(s%60));
  spr.drawString(buf, 6, y); y += lh;
  snprintf(buf, sizeof(buf), "MAC:  %s", mac);
  spr.drawString(buf, 6, y); y += lh;
  snprintf(buf, sizeof(buf), "Batt: %dmV", battMv);
  spr.drawString(buf, 6, y); y += lh;
  snprintf(buf, sizeof(buf), "SD:   %s", sdPresent ? "YES" : "NO");
  spr.drawString(buf, 6, y); y += lh;
  snprintf(buf, sizeof(buf), "CPU:  %dMHz", getCpuFrequencyMhz());
  spr.drawString(buf, 6, y);
}

/* ── Text Screen ───────────────────────────────────────── */
void DisplayManager::drawTextScreen(const char* title, const char* lines[],
                                    uint8_t lineCount) {
  drawMenuHeader(title);
  spr.setTextFont(1);
  spr.setTextColor(COL_TEXT, getBGColor());
  for (uint8_t i = 0; i < lineCount && i < 14; i++) {
    spr.drawString(lines[i], 6, MENU_Y_START + (i * 14));
  }
}

/* ── Confirm Dialog ────────────────────────────────────── */
void DisplayManager::drawConfirmDialog(const char* title, const char* msg) {
  uint16_t dw = TFT_SCREEN_WIDTH - 20;
  spr.fillRect(10, 80, dw, 100, COL_BG_NAVY);
  spr.drawRect(10, 80, dw, 100, COL_SEPARATOR);

  spr.setTextFont(2);
  spr.setTextColor(COL_TEXT, COL_BG_NAVY);
  spr.drawString(title, 16, 86);

  spr.setTextFont(1);
  spr.setTextColor(COL_TEXT_DIM, COL_BG_NAVY);
  spr.drawString(msg, 16, 110);

  spr.setTextColor(COL_STATUS_OK, COL_BG_NAVY);
  spr.drawString("SELECT = OK", 16, 145);
  spr.setTextColor(COL_TEXT_DIM, COL_BG_NAVY);
  spr.drawString("HOLD UP = Cancel", 16, 158);
}

/* ── Progress Bar ──────────────────────────────────────── */
void DisplayManager::drawProgressBar(uint16_t x, uint16_t y, uint16_t w,
                                     uint16_t h, uint8_t percent,
                                     uint16_t color) {
  spr.drawRect(x, y, w, h, COL_SEPARATOR);
  uint16_t fillW = (uint16_t)((percent / 100.0f) * (w - 2));
  spr.fillRect(x + 1, y + 1, fillW, h - 2, color);
}

/* ── RSSI Bar ──────────────────────────────────────────── */
void DisplayManager::drawRSSIBar(uint16_t x, uint16_t y, int8_t rssi) {
  int8_t norm = constrain(rssi, -100, -20);
  uint8_t bars = map(norm, -100, -20, 0, 4);
  uint16_t col = rssiColor(rssi);
  for (uint8_t i = 0; i < 4; i++) {
    uint16_t barH = 3 + (i * 2);
    uint16_t barY = y + (8 - barH);
    spr.fillRect(x + (i * 6), barY, 4, barH, (i < bars) ? col : COL_BAR_BG);
  }
}

/* ── Canvas ────────────────────────────────────────────── */
void DisplayManager::initCanvas() {
  spr.fillRect(0, STATUS_BAR_H, TFT_SCREEN_WIDTH,
               TFT_SCREEN_HEIGHT - STATUS_BAR_H, COL_BG);
}

void DisplayManager::drawPixel(uint16_t x, uint16_t y, uint16_t color) {
  if (y > STATUS_BAR_H) spr.fillRect(x - 1, y - 1, 3, 3, color);
}

/* ── Alert ─────────────────────────────────────────────── */
void DisplayManager::drawAlert(const char* msg, uint16_t color) {
  spr.fillRect(6, TFT_SCREEN_HEIGHT - 36, TFT_SCREEN_WIDTH - 12, 28, COL_BG_NAVY);
  spr.drawRect(6, TFT_SCREEN_HEIGHT - 36, TFT_SCREEN_WIDTH - 12, 28, color);
  spr.setTextFont(1);
  spr.setTextColor(color, COL_BG_NAVY);
  spr.drawString(msg, 12, TFT_SCREEN_HEIGHT - 30);
}
