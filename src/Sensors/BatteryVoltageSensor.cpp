#include "BatteryVoltageSensor.h"
#include "../config.h"

BatteryVoltageSensor::BatteryVoltageSensor(ReadFn readFn, float dividerRatio)
    : readFn(readFn), dividerRatio(dividerRatio), voltageMilliVolts(0), lastRead(0) {
}

void BatteryVoltageSensor::handle() {
    unsigned long now = millis();
    if (now - lastRead < 100) {
        return;
    }
    lastRead = now;
    readVoltage();
}

void BatteryVoltageSensor::readVoltage() {
    int oversampledValue = 0;
    for (int i = 0; i < oversampleCount; i++) {
        oversampledValue += readFn();
    }
    int adcValue = oversampledValue / oversampleCount;

    // Convert ADC reading to voltage at GPIO pin
    // ESP32-C3: 12-bit ADC (0-4095) with 3.3V reference
    double voltageAtPin = (ADC_VREF * (double)adcValue) / ADC_MAX_VALUE;

    // Calculate actual battery voltage using divider ratio
    // V_battery = V_pin * BATTERY_DIVIDER_RATIO
    double batteryVoltage = voltageAtPin * dividerRatio;

    // Convert to millivolts
    // Maximum expected: 60.0V, so 60.0 * 1000 = 60000 mV < 65535 (uint16_t max)
    voltageMilliVolts = (uint16_t)(batteryVoltage * 1000.0 + 0.5);
}
