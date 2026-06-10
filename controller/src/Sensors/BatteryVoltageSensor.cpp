#include "BatteryVoltageSensor.h"
#include "../config.h"

BatteryVoltageSensor::BatteryVoltageSensor(ReadFn readFn, float dividerRatio, float adcVoltageRef)
    : readFn(readFn), dividerRatio(dividerRatio), adcVoltageRef(adcVoltageRef), voltageMilliVolts(0), lastRead(0) {
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

    // Convert ADC reading to voltage at sensor pin
    // adcVoltageRef: 3.3V for ESP32 ADC, 4.096V for ADS1115 GAIN_ONE
    double voltageAtPin = (adcVoltageRef * (double)adcValue) / ADC_MAX_VALUE;

    // Calculate actual battery voltage using divider ratio
    // V_battery = V_pin * BATTERY_DIVIDER_RATIO
    double batteryVoltage = voltageAtPin * dividerRatio;

    // Convert to millivolts
    // Maximum expected: 60.0V, so 60.0 * 1000 = 60000 mV < 65535 (uint16_t max)
    voltageMilliVolts = (uint16_t)(batteryVoltage * 1000.0 + 0.5);
}
