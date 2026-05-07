/*=============================================================
 * BatteryInterface.cpp — Battery monitoring + fuel gauge
 *=============================================================*/
#include "BatteryInterface.h"

BatteryInterface battery;

void BatteryInterface::begin() {
    #if HAS_BATTERY
    analogSetAttenuation(ADC_11db);  // 0-3.3V range
    analogReadResolution(12);        // 12-bit (0-4095)
    pinMode(BATTERY_PIN, INPUT);
    pinMode(CHARGING_PIN, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);     // LED off (active low)
    update();
    Serial.printf("[BATT] Init: %dmV (%d%%) %s\n",
                  _voltage, _percent, _charging ? "CHG" : "DCHG");
    #endif
}

void BatteryInterface::update() {
    #if HAS_BATTERY
    uint32_t now = millis();
    if (now - _lastRead < READ_INTERVAL && _lastRead != 0) return;
    _lastRead = now;

    _voltage  = readRawVoltage();
    _percent  = voltageToPercent(_voltage);
    _charging = (digitalRead(CHARGING_PIN) == LOW); // TP4056 CHRG active low

    // Low battery LED blink
    if (_percent <= 10 && !_charging) {
        digitalWrite(LED_PIN, (millis() / 500) % 2 == 0 ? LOW : HIGH);
    } else {
        digitalWrite(LED_PIN, HIGH); // LED off
    }
    #endif
}

uint16_t BatteryInterface::getVoltage() { return _voltage; }

uint8_t BatteryInterface::getPercent() { return _percent; }

bool BatteryInterface::isCharging() { return _charging; }

bool BatteryInterface::isLowBattery() { return _percent <= 10; }

bool BatteryInterface::isCritical() { return _voltage <= CRIT_BAT_MV; }

String BatteryInterface::getStatusStr() {
    // Battery bar: 5 blocks representing 0-100%
    String bar = "";
    uint8_t blocks = _percent / 20;
    for (uint8_t i = 0; i < 5; i++) {
        bar += (i < blocks) ? "\x7F" : " "; // block char or space
    }
    char buf[16];
    snprintf(buf, sizeof(buf), "%s%d%%", _charging ? "+" : "", _percent);
    return bar + String(buf);
}

uint16_t BatteryInterface::readRawVoltage() {
    #if HAS_BATTERY
    // Average 16 samples for noise reduction
    uint32_t sum = 0;
    for (int i = 0; i < 16; i++) {
        sum += analogRead(BATTERY_PIN);
    }
    uint16_t raw = sum / 16;

    // Convert ADC to millivolts
    // ADC range: 0-4095 maps to 0-3300mV at the divider output
    // Actual battery voltage = divider_output * (R1+R2)/R2
    float dividerVoltage = (raw / 4095.0f) * 3300.0f;
    float battVoltage = dividerVoltage * ((float)(BATTERY_R1 + BATTERY_R2) / (float)BATTERY_R2);
    return (uint16_t)battVoltage;
    #else
    return 4200; // simulate full
    #endif
}

uint8_t BatteryInterface::voltageToPercent(uint16_t mv) {
    // Li-ion discharge curve approximation
    if (mv >= 4100) return 100;
    if (mv >= 3950) return 90;
    if (mv >= 3850) return 80;
    if (mv >= 3780) return 70;
    if (mv >= 3720) return 60;
    if (mv >= 3680) return 50;
    if (mv >= 3640) return 40;
    if (mv >= 3600) return 30;
    if (mv >= 3550) return 20;
    if (mv >= 3500) return 10;
    if (mv >= 3400) return 5;
    if (mv >= 3300) return 1;
    return 0;
}
