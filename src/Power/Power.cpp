#include "../Telemetry/TelemetryProvider.h"
#include "../Telemetry/TelemetryData.h"
#include "Power.h"
#include "../config.h"
#include "../Throttle/Throttle.h"
#include "../Temperature/Temperature.h"

extern Throttle throttle;
extern Temperature motorTemp;
// telemetry is declared in config.cpp after TelemetryProvider is fully defined
extern TelemetryProvider* telemetry;

Power::Power() {
    lastPowerCalculationTime = 0;
    pwm = ESC_MIN_PWM;
    power = 100;
    batteryPowerFloor = 100;
}

unsigned int Power::getPwm() {
    unsigned int throttleRaw = throttle.getThrottleRaw();
    unsigned int powerLimit = getPower(); // 0-100

    unsigned int throttleMin = throttle.getThrottlePinMin();
    unsigned int throttleMax = throttle.getThrottlePinMax();

    // Check if throttle is calibrated (min and max are different)
    if (throttleMin == throttleMax) {
        return ESC_MIN_PWM;
    }

    unsigned int allowedMax = throttleMin + ((throttleMax - throttleMin) * powerLimit) / 100;
    unsigned int effectiveRaw = constrain(throttleRaw, throttleMin, allowedMax);

    return map(
        effectiveRaw,
        throttleMin,
        throttleMax,
        ESC_MIN_PWM,
        ESC_MAX_PWM
    );
}

unsigned int Power::getPower() {
    if (millis() - lastPowerCalculationTime < 1000) {
        return power;
    }

    lastPowerCalculationTime = millis();
    power = calcPower();

    return power;
}

unsigned int Power::calcPower() {
    unsigned int batteryLimit = calcBatteryLimit();
    unsigned int motorTempLimit = calcMotorTempLimit();
    unsigned int escTempLimit = calcEscTempLimit();

    return min(min(batteryLimit, motorTempLimit), escTempLimit);
}

// Public wrapper for battery voltage reading (for backward compatibility)
// Returns voltage in decivolts (tenths of volts)
unsigned int Power::getBatteryVoltageDeciVolts() {
    if (!telemetry || !telemetry->getData()) {
        return 0;
    }
    // Convert millivolts to decivolts: mV / 100 = dV
    return (unsigned int)(telemetry->getData()->batteryVoltageMilliVolts / 100);
}

unsigned int Power::calcBatteryLimit() {
    if (!telemetry || !telemetry->getData()) {
        return 0;
    }

    TelemetryData* data = telemetry->getData();

    // XAG mode: battery voltage reading is not reliable, do not limit power
    // The voltage is available for telemetry but should not be used to control power output
    #if IS_XAG
    return 100;
    #else
    if (!data->isReady) {
        return 0;
    }

    // Compare millivolts directly with constants in millivolts
    uint32_t batteryMilliVolts = data->batteryVoltageMilliVolts;
    const unsigned int STEP_DECREASE = 5;

    if (batteryMilliVolts > BATTERY_MIN_VOLTAGE) {
        return batteryPowerFloor;
    }

    if (batteryPowerFloor < STEP_DECREASE) {
        batteryPowerFloor = 0;
        return batteryPowerFloor;
    }

    batteryPowerFloor = batteryPowerFloor - STEP_DECREASE;

    return batteryPowerFloor;
    #endif
}

unsigned int Power::calcMotorTempLimit() {
    // Use motor temperature from sensor (not from telemetry, as it's read separately)
    double readedMotorTemp = motorTemp.getTemperature();

    // Convert to millicelsius for comparison
    int32_t motorTempMilliCelsius = (int32_t)(readedMotorTemp * 1000.0);

    // Validate temperature reading (compare in millicelsius)
    if (motorTempMilliCelsius < MOTOR_TEMP_MIN_VALID || motorTempMilliCelsius > MOTOR_TEMP_MAX_VALID) {
        return 100; // Invalid sensor data
    }

    if (motorTempMilliCelsius < MOTOR_TEMP_REDUCTION_START) {
        return 100;
    }

    // Check if temperature range is valid (min != max)
    if (MOTOR_TEMP_REDUCTION_START == MOTOR_MAX_TEMP) {
        return 0; // If min == max, return 0 (maximum reduction)
    }

    return constrain(
        map(
            motorTempMilliCelsius,
            MOTOR_TEMP_REDUCTION_START,
            MOTOR_MAX_TEMP,
            100,
            0
        ),
        0,
        100
    );
}

unsigned int Power::calcEscTempLimit() {
    if (!telemetry || !telemetry->getData()) {
        return 100; // Allow other limits to control
    }

    TelemetryData* data = telemetry->getData();

    #if IS_XAG
    // XAG mode: use ESC temperature from telemetry (read from NTC sensor)
    int32_t escTempMilliCelsius = data->escTemperatureMilliCelsius;
    #else
    // CAN controllers: use ESC temperature from telemetry
    if (!data->isReady) {
        return 100; // ESC not ready, allow other limits to control
    }

    int32_t escTempMilliCelsius = data->escTemperatureMilliCelsius;
    #endif

    // Validate temperature reading (compare in millicelsius)
    if (escTempMilliCelsius < ESC_TEMP_MIN_VALID || escTempMilliCelsius > ESC_TEMP_MAX_VALID) {
        return 100; // Invalid sensor data
    }

    if (escTempMilliCelsius < ESC_TEMP_REDUCTION_START) {
        return 100;
    }

    // Check if temperature range is valid (min != max)
    if (ESC_TEMP_REDUCTION_START == ESC_MAX_TEMP) {
        return 0; // If min == max, return 0 (maximum reduction)
    }

    return constrain(
        map(
            escTempMilliCelsius,
            ESC_TEMP_REDUCTION_START,
            ESC_MAX_TEMP,
            100,
            0
        ),
        0,
        100
    );
}

void Power::resetBatteryPowerFloor() {
    batteryPowerFloor = 100;
}
