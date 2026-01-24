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
    resetRampLimiting();
    #if IS_XAG
    smoothStartActive = false;
    motorStoppedTime = 0;
    smoothStartBeginTime = 0;
    smoothStartInitialPwm = ESC_MIN_PWM;
    #endif
}

unsigned int Power::getPwm() {
    if (!throttle.isCalibrated()) {
        resetRampLimiting();
        return ESC_MIN_PWM;
    }

    unsigned int throttleRaw = throttle.getThrottleRaw();
    unsigned int powerLimit = getPower(); // 0-100

    unsigned int throttleMin = throttle.getThrottlePinMin();
    unsigned int throttleMax = throttle.getThrottlePinMax();

    unsigned int allowedMax = throttleMin + ((throttleMax - throttleMin) * powerLimit) / 100;
    unsigned int effectiveRaw = constrain(throttleRaw, throttleMin, allowedMax);

    int targetPwm = map(
        effectiveRaw,
        throttleMin,
        throttleMax,
        ESC_MIN_PWM,
        ESC_MAX_PWM
    );

    #if IS_XAG
    // If smooth start is active, check if we should continue
    if (smoothStartActive) {
        // If target PWM goes back to minimum, cancel smooth start
        if (targetPwm <= ESC_MIN_PWM) {
            smoothStartActive = false;
            prevPwm = ESC_MIN_PWM;
            return ESC_MIN_PWM;
        }
        return applySmoothStart(targetPwm);
    }

    // Check if motor is stopped
    bool motorStopped = detectMotorStopped();

    // If motor is stopped and target PWM is above minimum, activate smooth start
    if (motorStopped && targetPwm > ESC_MIN_PWM) {
        smoothStartActive = true;
        smoothStartBeginTime = millis();
        smoothStartInitialPwm = ESC_MIN_PWM;
        return applySmoothStart(targetPwm);
    }
    #endif

    // Apply ramp limiting to smooth acceleration/deceleration
    return applyRampLimiting(targetPwm);
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
    resetRampLimiting();
}

void Power::resetRampLimiting() {
    prevPwm = ESC_MIN_PWM;
    #if IS_XAG
    smoothStartActive = false;
    motorStoppedTime = 0;
    smoothStartBeginTime = 0;
    smoothStartInitialPwm = ESC_MIN_PWM;
    #endif
}

unsigned int Power::applyRampLimiting(int targetPwm) {
    int delta = targetPwm - prevPwm;
    int maxDelta = (delta > 0)
        ? THROTTLE_RAMP_RATE
        : (int)(THROTTLE_RAMP_RATE * THROTTLE_DECEL_MULTIPLIER);

    int limitedPwm = prevPwm + constrain(delta, -maxDelta, maxDelta);
    // Ensure the result is within valid PWM range
    limitedPwm = constrain(limitedPwm, ESC_MIN_PWM, ESC_MAX_PWM);
    prevPwm = limitedPwm;

    return (unsigned int)limitedPwm;
}

#if IS_XAG
bool Power::detectMotorStopped() {
    unsigned long now = millis();

    // Check if current PWM is at minimum
    if (prevPwm == ESC_MIN_PWM) {
        // If timer not started, start it
        if (motorStoppedTime == 0) {
            motorStoppedTime = now;
        }

        // Check if motor has been stopped for required time
        if (now - motorStoppedTime >= SMOOTH_START_MOTOR_STOPPED_TIME) {
            return true;
        }
    } else {
        // Motor is not at minimum, reset timer
        motorStoppedTime = 0;
    }

    return false;
}

unsigned int Power::applySmoothStart(int targetPwm) {
    unsigned long now = millis();

    // Calculate progress (0.0 to 1.0)
    unsigned long elapsed = now - smoothStartBeginTime;
    float progress = (float)elapsed / (float)SMOOTH_START_DURATION;

    // Clamp progress to 1.0
    if (progress >= 1.0f) {
        progress = 1.0f;
        smoothStartActive = false; // Smooth start complete
    }

    // Linear interpolation from ESC_MIN_PWM to targetPwm
    int smoothPwm = ESC_MIN_PWM + (int)((targetPwm - ESC_MIN_PWM) * progress);

    // Ensure the result is within valid PWM range
    smoothPwm = constrain(smoothPwm, ESC_MIN_PWM, ESC_MAX_PWM);

    // Update prevPwm for ramp limiting after smooth start
    prevPwm = smoothPwm;

    return (unsigned int)smoothPwm;
}
#endif
