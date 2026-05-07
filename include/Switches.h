/*=============================================================
 * Switches.h — Button debounce library for 3-button nav
 *=============================================================*/
#ifndef SWITCHES_H
#define SWITCHES_H

#include <Arduino.h>
#include "configs.h"

// Debounce constants
#define BTN_DEBOUNCE    50
#define BTN_HOLD_TIME   2000

enum ButtonEvent : uint8_t {
    BTN_NONE = 0,
    BTN_SHORT_PRESS,
    BTN_LONG_PRESS
};

class Switch {
public:
    Switch(uint8_t pin, uint16_t debounceMs = BTN_DEBOUNCE,
           uint16_t holdMs = BTN_HOLD_TIME)
        : _pin(pin), _debounceMs(debounceMs), _holdMs(holdMs),
          _lastState(HIGH), _stableState(HIGH),
          _lastDebounce(0), _pressStart(0),
          _pressed(false), _longFired(false) {}

    void begin() {
        pinMode(_pin, INPUT);
    }

    ButtonEvent update() {
        uint8_t reading = digitalRead(_pin);
        ButtonEvent evt = BTN_NONE;
        uint32_t now = millis();

        if (reading != _lastState) {
            _lastDebounce = now;
        }

        if ((now - _lastDebounce) > _debounceMs) {
            if (reading != _stableState) {
                _stableState = reading;

                if (_stableState == LOW) {
                    _pressed = true;
                    _longFired = false;
                    _pressStart = now;
                } else {
                    if (_pressed && !_longFired) {
                        evt = BTN_SHORT_PRESS;
                    }
                    _pressed = false;
                    _longFired = false;
                }
            }

            if (_pressed && !_longFired && (now - _pressStart) >= _holdMs) {
                evt = BTN_LONG_PRESS;
                _longFired = true;
            }
        }

        // Button stuck detection (>30s)
        if (_pressed && (now - _pressStart) > 30000) {
            _pressed = false;
            _longFired = true;
        }

        _lastState = reading;
        return evt;
    }

    bool isPressed() const { return _pressed; }

private:
    uint8_t  _pin;
    uint16_t _debounceMs;
    uint16_t _holdMs;
    uint8_t  _lastState;
    uint8_t  _stableState;
    uint32_t _lastDebounce;
    uint32_t _pressStart;
    bool     _pressed;
    bool     _longFired;
};

class ButtonManager {
public:
    Switch btnUp{BTN_UP};
    Switch btnSelect{BTN_SELECT};
    Switch btnDown{BTN_DOWN};

    void begin() {
        btnUp.begin();
        btnSelect.begin();
        btnDown.begin();
    }

    void update() {
        evtUp     = btnUp.update();
        evtSelect = btnSelect.update();
        evtDown   = btnDown.update();
    }

    ButtonEvent evtUp     = BTN_NONE;
    ButtonEvent evtSelect = BTN_NONE;
    ButtonEvent evtDown   = BTN_NONE;
};

#endif // SWITCHES_H
