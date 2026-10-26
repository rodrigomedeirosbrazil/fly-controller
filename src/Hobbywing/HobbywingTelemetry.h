#ifndef HobbywingTelemetry_h
#define HobbywingTelemetry_h

#include <stdint.h>
#include <Arduino.h>
#include "../Telemetry/SignalState.h"

/**
 * Hobbywing telemetry aggregator: battery voltage from the voltage divider
 * (ADS1115) and motor temperature from the NTC; current, RPM and ESC
 * temperature from HobbywingCan (DroneCAN status frames).
 */
class HobbywingTelemetry {
public:
    void update();

    bool hasData() const;
    uint16_t getBatteryVoltageMilliVolts() const;
    uint32_t getBatteryCurrentMilliAmps() const;
    uint16_t getRpm() const;
    int32_t getMotorTempMilliCelsius() const;
    int32_t getEscTempMilliCelsius() const;
    unsigned long getLastUpdate() const;

    SignalState getMotorTempState() const;
    SignalState getEscTempState() const;
    SignalState getBatteryVoltageState() const;

private:
    bool cachedHasData = false;
    uint16_t cachedBatteryVoltageMilliVolts = 0;
    uint32_t cachedBatteryCurrentMilliAmps = 0;
    uint16_t cachedRpm = 0;
    int32_t cachedMotorTempMilliCelsius = 0;
    int32_t cachedEscTempMilliCelsius = 0;
    unsigned long cachedLastUpdate = 0;
    SignalState cachedMotorTempState = SignalState::Invalid;
    SignalState cachedEscTempState = SignalState::Invalid;
    SignalState cachedBatteryVoltageState = SignalState::Invalid;
};

#endif
