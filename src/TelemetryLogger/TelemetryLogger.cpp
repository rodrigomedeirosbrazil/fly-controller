#include "TelemetryLogger.h"
#include "../config.h"
#include "../Telemetry/TelemetryAvailability.h"
#include "../Throttle/Throttle.h"
#include "../Power/Power.h"
#include "../BatteryMonitor/BatteryMonitor.h"
#include "../Logger/Logger.h"
#include "../BluetoothBms/BluetoothBms.h"
#include <cstdarg>
#include <cstdio>

extern Throttle throttle;
extern Power power;
extern BatteryMonitor batteryMonitor;
extern Logger logger;

namespace {
bool appendToBuffer(char* data, size_t size, size_t& used, const char* format, ...) {
    if (used >= size) {
        return false;
    }

    va_list args;
    va_start(args, format);
    const int written = vsnprintf(data + used, size - used, format, args);
    va_end(args);

    if (written < 0) {
        return false;
    }

    const size_t charsWritten = static_cast<size_t>(written);
    if (charsWritten >= (size - used)) {
        used = size - 1;
        data[used] = '\0';
        return false;
    }

    used += charsWritten;
    return true;
}
} // namespace

TelemetryLogger::TelemetryLogger() {
    lastUpdate = 0;
}

void TelemetryLogger::init() {
    logger.setHeader("battery_percent_cc,battery_percent_voltage,voltage,power_kw,throttle_percent,throttle_raw,power_percent,motor_temp,rpm,esc_current,esc_temp,battery_temp_max,cell_voltage_min_mv,cell_voltage_max_mv");
}

void TelemetryLogger::handle() {
    if (millis() - lastUpdate < UPDATE_INTERVAL) {
        return;
    }
    lastUpdate = millis();

    char data[LINE_BUFFER_SIZE] = {0};
    size_t used = 0;

    writeBatteryInfo(data, sizeof(data), used);
    writeThrottleInfo(data, sizeof(data), used);
    writeMotorInfo(data, sizeof(data), used);
    writeEscInfo(data, sizeof(data), used);
    writeBmsInfo(data, sizeof(data), used);
    appendToBuffer(data, sizeof(data), used, "\r\n");

    logger.log(data);
}

void TelemetryLogger::writeBatteryInfo(char* data, size_t size, size_t& used) {
    if (!telemetry.hasData()) {
        appendToBuffer(data, size, used, ",,,,");
        return;
    }

    uint8_t batteryPercentageCC = batteryMonitor.getSoC();
    uint8_t batteryPercentageVoltage = batteryMonitor.getSoCFromVoltage();

    uint16_t millivolts = telemetry.getBatteryVoltageMilliVolts();
    uint16_t volts = millivolts / 1000;
    uint16_t decimals = millivolts % 1000;

    appendToBuffer(data, size, used, "%u,%u,%u.", batteryPercentageCC, batteryPercentageVoltage, volts);
    if (decimals < 10) {
        appendToBuffer(data, size, used, "0");
    }
    appendToBuffer(data, size, used, "%u,", decimals);

    if (isPowerKwAvailable()) {
        uint32_t powerMilliWatts = ((uint32_t)millivolts * telemetry.getBatteryCurrentMilliAmps()) / 1000;
        uint32_t powerKwInt = powerMilliWatts / 1000000;
        uint32_t powerKwDecimal = (powerMilliWatts / 100000) % 10;
        appendToBuffer(data, size, used, "%lu.%lu", (unsigned long)powerKwInt, (unsigned long)powerKwDecimal);
    }
    appendToBuffer(data, size, used, ",");
}

void TelemetryLogger::writeThrottleInfo(char* data, size_t size, size_t& used) {
    unsigned int throttlePercentage = throttle.getThrottlePercentage();
    unsigned int throttleRaw = throttle.getThrottleRaw();
    unsigned int powerPercentage = power.getPower();

    appendToBuffer(data, size, used, "%u,%u,%u,", throttlePercentage, throttleRaw, powerPercentage);
}

void TelemetryLogger::writeMotorInfo(char* data, size_t size, size_t& used) {
    if (!telemetry.hasData()) {
        appendToBuffer(data, size, used, ",");
    } else {
        int32_t tempCelsius = telemetry.getMotorTempMilliCelsius() / 1000;
        appendToBuffer(data, size, used, "%ld", (long)tempCelsius);
    }
    appendToBuffer(data, size, used, ",");

    if (isCurrentAvailable()) {
        appendToBuffer(
            data,
            size,
            used,
            "%lu,%lu",
            (unsigned long)telemetry.getRpm(),
            (unsigned long)(telemetry.getBatteryCurrentMilliAmps() / 1000)
        );
    } else {
        appendToBuffer(data, size, used, ",");
    }
    appendToBuffer(data, size, used, ",");
}

void TelemetryLogger::writeEscInfo(char* data, size_t size, size_t& used) {
    if (!telemetry.hasData()) {
        appendToBuffer(data, size, used, "");
        return;
    }

    int32_t escTempCelsius = telemetry.getEscTempMilliCelsius() / 1000;
    appendToBuffer(data, size, used, "%ld", (long)escTempCelsius);
}

void TelemetryLogger::writeBmsInfo(char* data, size_t size, size_t& used) {
    if (bluetoothBms.hasData() && bluetoothBms.getTempCount() > 0) {
        int16_t maxTemp = bluetoothBms.getTempCelsius(0);
        for (uint8_t i = 1; i < bluetoothBms.getTempCount(); i++) {
            int16_t t = bluetoothBms.getTempCelsius(i);
            if (t > maxTemp) maxTemp = t;
        }
        appendToBuffer(data, size, used, ",%d", maxTemp);
    } else {
        appendToBuffer(data, size, used, ",");
    }
    if (bluetoothBms.hasCellData()) {
        appendToBuffer(
            data,
            size,
            used,
            ",%u,%u",
            bluetoothBms.getCellMinMilliVolts(),
            bluetoothBms.getCellMaxMilliVolts()
        );
    } else {
        appendToBuffer(data, size, used, ",,");
    }
}
