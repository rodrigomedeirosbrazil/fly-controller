#ifndef TmotorTelemetry_h
#define TmotorTelemetry_h

#include <stdint.h>
#include <Arduino.h>

/**
 * Tmotor telemetry aggregator: TmotorCan (CAN) + motorTemp (ADS1115)
 */
class TmotorTelemetry {
public:
    void update();

    bool hasData() const;
    uint16_t getBatteryVoltageMilliVolts() const;
    uint32_t getBatteryCurrentMilliAmps() const;
    uint16_t getRpm() const;
    int32_t getMotorTempMilliCelsius() const;
    int32_t getEscTempMilliCelsius() const;
    unsigned long getLastUpdate() const;

private:
    bool cachedHasData = false;
    uint16_t cachedBatteryVoltageMilliVolts = 0;
    uint32_t cachedBatteryCurrentMilliAmps = 0;
    uint16_t cachedRpm = 0;
    int32_t cachedMotorTempMilliCelsius = 0;
    int32_t cachedEscTempMilliCelsius = 0;
    unsigned long cachedLastUpdate = 0;
};

#endif
