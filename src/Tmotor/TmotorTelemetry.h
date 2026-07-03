#ifndef TmotorTelemetry_h
#define TmotorTelemetry_h

#include <stdint.h>
#include <Arduino.h>

/**
 * Tmotor telemetry aggregator: battery voltage from ADS1115; current/RPM/ESC temp from TmotorCan (CAN).
 * Motor temperature: CAN (Status 5 / PUSHCAN) when recently received; else NTC/ADS1115 fallback (ESC may not send motor temp).
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
