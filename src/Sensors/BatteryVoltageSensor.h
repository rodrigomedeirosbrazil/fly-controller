#ifndef BATTERY_VOLTAGE_SENSOR_H
#define BATTERY_VOLTAGE_SENSOR_H

#include <stdint.h>
#include "../ADS1115/SensorReadingValidity.h"

class BatteryVoltageSensor {
public:
    typedef int (*ReadFn)();
    typedef bool (*ReadOkFn)();
    BatteryVoltageSensor(ReadFn readFn, ReadOkFn readOkFn, float dividerRatio, float adcVoltageRef);
    void handle();
    uint16_t getVoltageMilliVolts() const { return voltageMilliVolts; }
    void setDividerRatio(float ratio) { dividerRatio = ratio; }
    bool isValid() const { return valid; }

private:
    static constexpr unsigned long READ_INTERVAL_MS = 500;
    static constexpr float EMA_ALPHA = 0.3f;

    // Physically implausible battery-voltage bounds, in millivolts. A
    // disconnected divider reads near 0V (R2 pulls the tap to ground with no
    // path from the battery once R1 opens); no real 14S LiPo pack reads
    // above ~65.8V even during an abusive overcharge. The upper bound is
    // also capped below the uint16_t ceiling (65535) that
    // voltageMilliVolts's storage type imposes, so it stays reachable —
    // see docs/superpowers/specs/2026-08-01-signal-validity-design.md.
    enum : int { VALID_MIN_MV = 5000, VALID_MAX_MV = 65000 };

    ReadFn readFn;
    ReadOkFn readOkFn;
    float dividerRatio;
    float adcVoltageRef;
    uint16_t voltageMilliVolts;
    bool valid;
    SensorReadingValidity validity;
    unsigned long lastRead;
    float emaVoltageMilliVolts;
    bool emaInitialized;

    void readVoltage();
};

#endif // BATTERY_VOLTAGE_SENSOR_H
