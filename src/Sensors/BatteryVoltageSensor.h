#ifndef BATTERY_VOLTAGE_SENSOR_H
#define BATTERY_VOLTAGE_SENSOR_H

#include <stdint.h>

class BatteryVoltageSensor {
public:
    typedef int (*ReadFn)();
    BatteryVoltageSensor(ReadFn readFn, float dividerRatio, float adcVoltageRef);
    void handle();
    uint16_t getVoltageMilliVolts() const { return voltageMilliVolts; }

private:
    static const int oversampleCount = 10;

    ReadFn readFn;
    float dividerRatio;
    float adcVoltageRef;
    uint16_t voltageMilliVolts;
    unsigned long lastRead;

    void readVoltage();
};

#endif // BATTERY_VOLTAGE_SENSOR_H
