#ifndef TELEMETRY_FORMATTER_H
#define TELEMETRY_FORMATTER_H

#include <Arduino.h>

/**
 * Format voltage from millivolts to string (e.g., 44100 -> "44.100")
 * Uses integer arithmetic to avoid float conversion
 */
inline String formatVoltage(uint16_t mV) {
    uint16_t volts = mV / 1000;
    uint16_t decimals = mV % 1000;
    String result = String(volts);
    result += ".";
    if (decimals < 10) {
        result += "00";
    } else if (decimals < 100) {
        result += "0";
    }
    result += String(decimals);
    return result;
}

/**
 * Format temperature from millicelsius to string (e.g., 50000 -> "50")
 * Uses integer arithmetic to avoid float conversion
 */
inline String formatTemperature(int32_t mC) {
    int32_t celsius = mC / 1000;
    return String(celsius);
}

/**
 * Format temperature from millicelsius to string with decimals (e.g., 50123 -> "50.123")
 * Uses integer arithmetic to avoid float conversion
 */
inline String formatTemperatureWithDecimals(int32_t mC) {
    int32_t celsius = mC / 1000;
    int32_t decimals = abs(mC % 1000);
    String result = String(celsius);
    result += ".";
    if (decimals < 10) {
        result += "00";
    } else if (decimals < 100) {
        result += "0";
    }
    result += String(decimals);
    return result;
}

#endif // TELEMETRY_FORMATTER_H
