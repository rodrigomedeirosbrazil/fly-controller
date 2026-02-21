#include <Arduino.h>
#include "Throttle.h"

#include "../config.h"

Throttle::Throttle(ReadFn readFn) : readFn(readFn) {
  memset(
    &pinValues,
    0,
    sizeof(pinValues[0]) * samples
  );

  pinValueFiltered = 0;
  lastThrottleRead = 0;

  throttleArmed = false;
  resetCalibration();

  throttlePinMin = 0;
  throttlePinMax = 0;

  armingTries = 0;
}

void Throttle::handle()
{
  unsigned long now = millis();

  if (now - lastThrottleRead < 10) {
    return;
  }

  lastThrottleRead = now;
  readThrottlePin();

  // Handle calibration if not yet calibrated
  if (!calibrated) {
    handleCalibration(now);
  }
}

void Throttle::resetCalibration()
{
  calibrated = false;
  calibratingStep = 0;
  calibrationStartTime = 0;
  calibrationMaxValue = 0;
  calibrationMinValue = ADC_MAX_VALUE; // Start with max possible value for ADC (4095 for 12-bit)
  calibrationSumMax = 0;
  calibrationCountMax = 0;
  calibrationSumMin = 0;
  calibrationCountMin = 0;
}

void Throttle::handleCalibration(unsigned long now)
{
  // Step 0: Calibrate maximum throttle
  if (calibratingStep == 0) {
    // Check if throttle is above threshold
    if (pinValueFiltered > calibrationThreshold) {
      // Start timing if not already started
      if (calibrationStartTime == 0) {
        calibrationStartTime = now;
        calibrationSumMax = 0;
        calibrationCountMax = 0;
      }

      // Accumulate values for averaging
      calibrationSumMax += pinValueFiltered;
      calibrationCountMax++;

      // Track the maximum value seen (kept for compatibility, but not used anymore)
      if (pinValueFiltered > calibrationMaxValue) {
        calibrationMaxValue = pinValueFiltered;
      }

      // Check if we've held the throttle for the required time
      if (now - calibrationStartTime >= calibrationTime && calibrationCountMax > 0) {
        // Set the max throttle value as the average
        throttlePinMax = calibrationSumMax / calibrationCountMax;

        buzzer.beepCalibrationStep();

        // Move to next step
        calibratingStep = 1;
        calibrationStartTime = 0; // Reset timer for next step
      }
      return;
    }

    // Reset timer and accumulators if throttle drops below threshold
    calibrationStartTime = 0;
    calibrationSumMax = 0;
    calibrationCountMax = 0;
    return;
  }

  // Step 1: Calibrate minimum throttle
  if (calibratingStep == 1) {
    // Check if throttle is below threshold
    if (pinValueFiltered < calibrationThreshold) {
      // Start timing if not already started
      if (calibrationStartTime == 0) {
        calibrationStartTime = now;
        calibrationSumMin = 0;
        calibrationCountMin = 0;
      }

      // Accumulate values for averaging
      calibrationSumMin += pinValueFiltered;
      calibrationCountMin++;

      // Track the minimum value seen (kept for compatibility, but not used anymore)
      if (pinValueFiltered < calibrationMinValue) {
        calibrationMinValue = pinValueFiltered;
      }

      // Check if we've held the throttle for the required time
      if (now - calibrationStartTime >= calibrationTime && calibrationCountMin > 0) {
        // Set the min throttle value as the average
        throttlePinMin = calibrationSumMin / calibrationCountMin;

        // Calibration complete
        calibrated = true;
        buzzer.beepCalibrationComplete();
        return;
      }

      return;
    }

    // Reset timer and accumulators if throttle goes above threshold
    calibrationStartTime = 0;
    calibrationSumMin = 0;
    calibrationCountMin = 0;
    return;
  }
}

void Throttle::readThrottlePin()
{
  memcpy(
    &pinValues,
    &pinValues[1],
    sizeof(pinValues[0]) * (samples - 1)
  );

  int oversampledValue = readFn();
  pinValues[samples - 1] = oversampledValue;

  // Calculate moving average
  int sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += pinValues[i];
  }

  pinValueFiltered = sum / samples;
}

unsigned int Throttle::getThrottlePercentage()
{
  // Check if throttle is calibrated (min and max are different)
  if (!calibrated || throttlePinMin == throttlePinMax) {
    return 0;
  }

  int pinValueConstrained = getThrottleRaw();
  unsigned int throttlePercentage = map(pinValueConstrained, throttlePinMin, throttlePinMax, 0, 100);

  if (throttlePercentage < 5) {
    return 0;
  }

  if (throttlePercentage > 95) {
    return 100;
  }

  return throttlePercentage;
}

unsigned int Throttle::getThrottleRaw()
{
  int pinValueConstrained = constrain(pinValueFiltered, throttlePinMin, throttlePinMax);
  return pinValueConstrained;
}

void Throttle::setArmed()
{
  power.resetBatteryPowerFloor();
  if (throttleArmed) {
    return;
  }

  // Don't allow arming if not calibrated
  if (! calibrated) {
    return;
  }

  if (getThrottlePercentage() > 0) {
    buzzer.beepArmingBlocked();
    armingTries++;

    if (armingTries > 2) {
      armingTries = 0;
      resetCalibration();
    }

    return;
  }

  throttleArmed = true;
#if USES_CAN_BUS && IS_HOBBYWING
  extern HobbywingCan hobbywingCan;
  if (hobbywingCan.isReady()) {
    hobbywingCan.setLedColor(HobbywingCan::ledColorRed, HobbywingCan::ledBlink5Hz);
  }
#endif
}

void Throttle::setDisarmed()
{
  throttleArmed = false;
  buzzer.beepDisarmed();
#if USES_CAN_BUS && IS_HOBBYWING
  extern HobbywingCan hobbywingCan;
  if (hobbywingCan.isReady()) {
    hobbywingCan.setLedColor(HobbywingCan::ledColorGreen);
  }
#endif
}
