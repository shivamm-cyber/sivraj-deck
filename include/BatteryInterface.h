/*=============================================================
 * BatteryInterface.h — Battery monitoring + fuel gauge
 *=============================================================*/
#ifndef BATTERY_INTERFACE_H
#define BATTERY_INTERFACE_H

#include <Arduino.h>
#include "configs.h"

class BatteryInterface {
public:
    void begin();
    void update();     // call periodically (every 1s)

    uint16_t getVoltage();    // mV
    uint16_t getVoltageMV() { return getVoltage(); } // alias
    uint8_t  getPercent();    // 0-100
    bool     isCharging();
    bool     isLowBattery();  // <10%
    bool     isCritical();    // <3.2V
    String   getStatusStr();  // "████87%"

private:
    uint16_t _voltage = 0;
    uint8_t  _percent = 100;
    bool     _charging = false;
    uint32_t _lastRead = 0;

    static const uint16_t READ_INTERVAL = 2000; // ms

    uint16_t readRawVoltage();
    uint8_t  voltageToPercent(uint16_t mv);
};

extern BatteryInterface battery;

#endif // BATTERY_INTERFACE_H
