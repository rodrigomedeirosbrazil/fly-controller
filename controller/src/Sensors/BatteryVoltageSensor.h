#ifndef BATTERY_VOLTAGE_SENSOR_H
#define BATTERY_VOLTAGE_SENSOR_H

#include <stdint.h>

class BatteryVoltageSensor {
public:
    typedef int (*ReadFn)();
    BatteryVoltageSensor(ReadFn readFn, float dividerRatio, float adcVoltageRef);
    void handle();
    uint16_t getVoltageMilliVolts() const { return voltageMilliVolts; }
    void setDividerRatio(float ratio) { dividerRatio = ratio; }

private:
    static constexpr unsigned long READ_INTERVAL_MS = 500;
    static constexpr float EMA_ALPHA = 0.3f;

    ReadFn readFn;
    float dividerRatio;
    float adcVoltageRef;
    uint16_t voltageMilliVolts;
    unsigned long lastRead;
    float emaVoltageMilliVolts;
    bool emaInitialized;

    void readVoltage();
};

#endif // BATTERY_VOLTAGE_SENSOR_H
