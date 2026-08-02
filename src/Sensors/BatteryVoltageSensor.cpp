#include "BatteryVoltageSensor.h"
#include "../config.h"

BatteryVoltageSensor::BatteryVoltageSensor(ReadFn readFn, ReadOkFn readOkFn, float dividerRatio, float adcVoltageRef)
    : readFn(readFn), readOkFn(readOkFn), dividerRatio(dividerRatio), adcVoltageRef(adcVoltageRef),
      voltageMilliVolts(0), valid(false), lastRead(0), emaVoltageMilliVolts(0.0f), emaInitialized(false) {
}

void BatteryVoltageSensor::handle() {
    unsigned long now = millis();
    if (now - lastRead < READ_INTERVAL_MS) {
        return;
    }
    lastRead = now;
    readVoltage();
}

void BatteryVoltageSensor::readVoltage() {
    int adcValue = readFn();
    validity.recordSample(readOkFn());

    // Convert ADC reading to voltage at sensor pin
    // adcVoltageRef: 3.3V for ESP32 ADC, 4.096V for ADS1115 GAIN_ONE
    double voltageAtPin = (adcVoltageRef * (double)adcValue) / ADC_MAX_VALUE;

    // Calculate actual battery voltage using divider ratio
    // V_battery = V_pin * BATTERY_DIVIDER_RATIO
    double batteryVoltage = voltageAtPin * dividerRatio;

    // Convert to millivolts
    // Maximum expected: 60.0V, so 60.0 * 1000 = 60000 mV < 65535 (uint16_t max)
    float rawMilliVolts = (float)(batteryVoltage * 1000.0);

    if (!emaInitialized) {
        emaVoltageMilliVolts = rawMilliVolts;
        emaInitialized = true;
    } else {
        emaVoltageMilliVolts += EMA_ALPHA * (rawMilliVolts - emaVoltageMilliVolts);
    }

    voltageMilliVolts = (uint16_t)(emaVoltageMilliVolts + 0.5f);
    valid = validity.isValid((int)voltageMilliVolts, VALID_MIN_MV, VALID_MAX_MV);
}
