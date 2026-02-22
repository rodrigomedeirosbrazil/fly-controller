#include "Power.h"
#include "../config.h"
#include "../Throttle/Throttle.h"
#include "../Temperature/Temperature.h"

extern Throttle throttle;
extern Temperature motorTemp;
extern Settings settings;

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
    preStartActive = false;
    preStartBeginTime = 0;
    // Calculate pre-start PWM: 5% of range
    preStartPwm = ESC_MIN_PWM + (SMOOTH_START_PRE_START_PERCENT * (ESC_MAX_PWM - ESC_MIN_PWM)) / 100;
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
    // If pre-start is active, apply pre-start phase
    if (preStartActive) {
        // If target PWM goes back to minimum, cancel pre-start
        if (targetPwm <= ESC_MIN_PWM) {
            preStartActive = false;
            prevPwm = ESC_MIN_PWM;
            return ESC_MIN_PWM;
        }
        // applyPreStart() will handle transition to smooth start when duration completes
        return applyPreStart();
    }

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

    // If motor is stopped and target PWM is above minimum, activate pre-start
    if (motorStopped && targetPwm > ESC_MIN_PWM) {
        preStartActive = true;
        preStartBeginTime = millis();
        return applyPreStart();
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
    // Check if power control is enabled
    if (!settings.getPowerControlEnabled()) {
        return 100; // No limitations when power control is disabled
    }

    unsigned int batteryLimit = calcBatteryLimit();
    unsigned int motorTempLimit = calcMotorTempLimit();
    unsigned int escTempLimit = calcEscTempLimit();

    return min(min(batteryLimit, motorTempLimit), escTempLimit);
}

unsigned int Power::calcBatteryLimit() {
    // XAG mode: battery voltage reading is not reliable, do not limit power
    // The voltage is available for telemetry but should not be used to control power output
    #if IS_XAG
    return 100;
    #else
    if (!telemetry.hasData()) {
        return 0;
    }

    // Compare millivolts directly with settings value in millivolts
    uint16_t batteryMilliVolts = telemetry.getBatteryVoltageMilliVolts();
    const unsigned int STEP_DECREASE = 5;

    if (batteryMilliVolts > settings.getBatteryMinVoltage()) {
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
    int32_t motorTempMilliCelsius;

    // Use motor temperature from sensor (ADC)
    double readedMotorTemp = motorTemp.getTemperature();
    motorTempMilliCelsius = (int32_t)(readedMotorTemp * 1000.0);

    // Validate temperature reading (compare in millicelsius)
    if (motorTempMilliCelsius < MOTOR_TEMP_MIN_VALID || motorTempMilliCelsius > MOTOR_TEMP_MAX_VALID) {
        return 100; // Invalid sensor data
    }

    int32_t motorTempReductionStart = settings.getMotorTempReductionStart();
    int32_t motorMaxTemp = settings.getMotorMaxTemp();

    if (motorTempMilliCelsius < motorTempReductionStart) {
        return 100;
    }

    // Check if temperature range is valid (min != max)
    if (motorTempReductionStart == motorMaxTemp) {
        return 0; // If min == max, return 0 (maximum reduction)
    }

    return constrain(
        map(
            motorTempMilliCelsius,
            motorTempReductionStart,
            motorMaxTemp,
            100,
            0
        ),
        0,
        100
    );
}

unsigned int Power::calcEscTempLimit() {
    #if IS_XAG
    // XAG mode: use ESC temperature from telemetry (read from NTC sensor)
    int32_t escTempMilliCelsius = telemetry.getEscTempMilliCelsius();
    #else
    // CAN controllers: use ESC temperature from telemetry
    if (!telemetry.hasData()) {
        return 100; // Telemetry not available, allow other limits to control
    }
    int32_t escTempMilliCelsius = telemetry.getEscTempMilliCelsius();
    #endif

    // Validate temperature reading (compare in millicelsius)
    if (escTempMilliCelsius < ESC_TEMP_MIN_VALID || escTempMilliCelsius > ESC_TEMP_MAX_VALID) {
        return 100; // Invalid sensor data
    }

    int32_t escTempReductionStart = settings.getEscTempReductionStart();
    int32_t escMaxTemp = settings.getEscMaxTemp();

    if (escTempMilliCelsius < escTempReductionStart) {
        return 100;
    }

    // Check if temperature range is valid (min != max)
    if (escTempReductionStart == escMaxTemp) {
        return 0; // If min == max, return 0 (maximum reduction)
    }

    return constrain(
        map(
            escTempMilliCelsius,
            escTempReductionStart,
            escMaxTemp,
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
    preStartActive = false;
    preStartBeginTime = 0;
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

unsigned int Power::applyPreStart() {
    unsigned long now = millis();

    // Check if pre-start duration has elapsed
    if (now - preStartBeginTime >= SMOOTH_START_PRE_START_DURATION) {
        // Pre-start complete, transition to smooth start
        preStartActive = false;
        smoothStartActive = true;
        smoothStartBeginTime = now;
        smoothStartInitialPwm = preStartPwm;
    }

    // Update prevPwm to preStartPwm
    prevPwm = preStartPwm;

    // Return fixed 5% PWM value
    return (unsigned int)preStartPwm;
}

unsigned int Power::applySmoothStart(int targetPwm) {
    unsigned long now = millis();

    // Calculate progress in permille (0 to 1000) using integer arithmetic
    unsigned long elapsed = now - smoothStartBeginTime;
    uint16_t progressPermille = (elapsed * 1000) / SMOOTH_START_DURATION;

    // Clamp progress to 1000 (100%)
    if (progressPermille >= 1000) {
        progressPermille = 1000;
        smoothStartActive = false; // Smooth start complete
    }

    // Linear interpolation from smoothStartInitialPwm (preStartPwm) to targetPwm
    // Using integer arithmetic: smoothPwm = initial + ((target - initial) * progressPermille) / 1000
    int smoothPwm = smoothStartInitialPwm + ((targetPwm - smoothStartInitialPwm) * progressPermille) / 1000;

    // Ensure the result is within valid PWM range
    smoothPwm = constrain(smoothPwm, ESC_MIN_PWM, ESC_MAX_PWM);

    // Update prevPwm for ramp limiting after smooth start
    prevPwm = smoothPwm;

    return (unsigned int)smoothPwm;
}
#endif
