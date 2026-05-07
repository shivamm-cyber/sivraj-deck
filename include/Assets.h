/*=============================================================
 * Assets.h — Minimal icon bitmap data (text-only menu)
 * Only small status bar indicator bitmaps kept here
 *=============================================================*/
#ifndef ASSETS_H
#define ASSETS_H

#include <Arduino.h>

// WiFi signal icon 8x8 bitmap (1-bit)
static const uint8_t PROGMEM icon_wifi[] = {
    0b00000000,
    0b00111100,
    0b01000010,
    0b10011001,
    0b00100100,
    0b01000010,
    0b00011000,
    0b00011000
};

// Bluetooth icon 8x8
static const uint8_t PROGMEM icon_bt[] = {
    0b00001000,
    0b00001010,
    0b01001100,
    0b00101000,
    0b00101000,
    0b01001100,
    0b00001010,
    0b00001000
};

// Battery icon 12x8
static const uint8_t PROGMEM icon_batt_full[] = {
    0b01111111, 0b11100000,
    0b10000000, 0b00010000,
    0b10111011, 0b10110000,
    0b10111011, 0b10110000,
    0b10111011, 0b10110000,
    0b10111011, 0b10110000,
    0b10000000, 0b00010000,
    0b01111111, 0b11100000
};

// SD card icon 8x8
static const uint8_t PROGMEM icon_sd[] = {
    0b00111110,
    0b01000010,
    0b01010010,
    0b01010010,
    0b01000010,
    0b01111110,
    0b01111110,
    0b01111110
};

// Up arrow 8x8
static const uint8_t PROGMEM icon_arrow_up[] = {
    0b00011000,
    0b00111100,
    0b01111110,
    0b11111111,
    0b00011000,
    0b00011000,
    0b00011000,
    0b00011000
};

// Down arrow 8x8
static const uint8_t PROGMEM icon_arrow_down[] = {
    0b00011000,
    0b00011000,
    0b00011000,
    0b00011000,
    0b11111111,
    0b01111110,
    0b00111100,
    0b00011000
};

// Warning icon 8x8
static const uint8_t PROGMEM icon_warning[] = {
    0b00011000,
    0b00011000,
    0b00111100,
    0b00111100,
    0b01111110,
    0b01100110,
    0b11111111,
    0b11111111
};

#endif // ASSETS_H
