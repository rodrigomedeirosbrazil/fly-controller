#ifndef XagTelemetry_h
#define XagTelemetry_h

#include <stdint.h>
#include <Arduino.h>

/**
 * XAG telemetry aggregator: motorTemp, escTemp, batterySensor (all ADC/ADS1115)
 */
class XagTelemetry {
public:
    void update();

    bool hasData() const;
    uint16_t getBatteryVoltageMilliVolts() const;
    uint32_t getBatteryCurrentMilliAmps() const { return 0; }
    uint16_t getRpm() const { return 0; }
    int32_t getMotorTempMilliCelsius() const;
    int32_t getEscTempMilliCelsius() const;
    unsigned long getLastUpdate() const;

private:
    bool cachedHasData = false;
    uint16_t cachedBatteryVoltageMilliVolts = 0;
    int32_t cachedMotorTempMilliCelsius = 0;
    int32_t cachedEscTempMilliCelsius = 0;
    unsigned long cachedLastUpdate = 0;
};

#endif
