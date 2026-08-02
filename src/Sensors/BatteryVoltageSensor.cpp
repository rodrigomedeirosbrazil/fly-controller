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
    bool readOk = readOkFn();
    validity.recordSample(readOk);

    if (!readOk) {
        // Don't blend a failed read's frozen/garbage value into the EMA —
        // hold the previous reading and let the I2C fail-streak alone
        // decide validity for this tick. Blending it in would let a single
        // bad read (which ADS1115::readChannel() answers with a stale
        // cached value, not a clearly-wrong one) quietly drag a real
        // reading toward garbage over several ticks.
        valid = validity.isValid((int)voltageMilliVolts, VALID_MIN_MV, VALID_MAX_MV);
        return;
    }

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

    // Clamp before narrowing to uint16_t — this sensor's physical ceiling
    // (dividerRatio * ADS1115 full scale) can exceed 65535mV at the default
    // ratio (23.13 * 4.096V ≈ 94.7V), and narrowing first would let a
    // railed/over-range reading wrap back into the valid band instead of
    // correctly failing it. Clamping saturates at 65535, which is above
    // VALID_MAX_MV, so an over-range reading still fails isValid() as it
    // should.
    float clampedMv = emaVoltageMilliVolts;
    if (clampedMv < 0.0f) clampedMv = 0.0f;
    if (clampedMv > 65535.0f) clampedMv = 65535.0f;
    voltageMilliVolts = (uint16_t)(clampedMv + 0.5f);
    valid = validity.isValid((int)voltageMilliVolts, VALID_MIN_MV, VALID_MAX_MV);
}
