#include <Arduino.h>
#include "Throttle.h"

#include "../config.h"

Throttle::Throttle() {
  memset(
    &pinValues,
    0,
    sizeof(pinValues[0]) * samples
  );

  pinValueFiltered = 0;
  lastThrottleRead = 0;

  throttleArmed = false;
  resetCalibration();

  cruising = false;
  cruisingThrottlePosition = 0;
  lastThrottlePosition = 0;
  timeThrottlePosition = 0;

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
  calibrationMinValue = 1023; // Start with max possible value for Arduino analog read
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
      }

      // Track the maximum value seen
      if (pinValueFiltered > calibrationMaxValue) {
        calibrationMaxValue = pinValueFiltered;
      }

      // Check if we've held the throttle for the required time
      if (now - calibrationStartTime >= calibrationTime) {
        // Set the max throttle value
        throttlePinMax = calibrationMaxValue;

        buzzer.beepSuccess();

        // Move to next step
        calibratingStep = 1;
        calibrationStartTime = 0; // Reset timer for next step
      }
      return;
    }

    // Reset timer if throttle drops below threshold
    calibrationStartTime = 0;
    return;
  }

  // Step 1: Calibrate minimum throttle
  if (calibratingStep == 1) {
    // Check if throttle is below threshold
    if (pinValueFiltered < calibrationThreshold) {
      // Start timing if not already started
      if (calibrationStartTime == 0) {
        calibrationStartTime = now;
      }

      // Track the minimum value seen
      if (pinValueFiltered < calibrationMinValue) {
        calibrationMinValue = pinValueFiltered;
      }

      // Check if we've held the throttle for the required time
      if (now - calibrationStartTime >= calibrationTime) {
        buzzer.beepSuccess();

        // Set the min throttle value
        throttlePinMin = calibrationMinValue;

        // Calibration complete
        calibrated = true;
        return;
      }

      return;
    }

    // Reset timer if throttle goes above threshold
    calibrationStartTime = 0;
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

  pinValues[samples - 1] = analogRead(THROTTLE_PIN);

  int sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += pinValues[i];
  }

  pinValueFiltered = sum / samples;
}

void Throttle::checkIfChangedCruiseState()
{
  if (! throttleArmed) {
    return;
  }

  unsigned long now = millis();
  unsigned int throttlePercentage = getThrottlePercentage();

  if (!cruising) {
    if (throttlePercentage < minCrusingThrottle) {
      return;
    }

    if (
      throttlePercentage > lastThrottlePosition + throttleRange
      || throttlePercentage < lastThrottlePosition - throttleRange
    ) {
      timeThrottlePosition = now;
      lastThrottlePosition = throttlePercentage;

      return;
    }

    if ((now - timeThrottlePosition) > timeToBeOnCruising) {
      setCruising(throttlePercentage);
      return;
    }

    return;
  }

  if (
    throttlePercentage < lastThrottlePosition + throttleRange
    && throttlePercentage > throttleRange
  ) {
    lastThrottlePosition = throttlePercentage < throttleRange
      ? throttleRange
      : throttlePercentage + throttleRange;

    return;
  }

  if (throttlePercentage > lastThrottlePosition) {
    cancelCruise();
    return;
  }
}

void Throttle::setCruising(int throttlePosition)
{
  cruisingThrottlePosition = throttlePosition;
  cruising = true;
}

unsigned int Throttle::getThrottlePercentage()
{
  int pinValueConstrained = constrain(pinValueFiltered, throttlePinMin, throttlePinMax);
  unsigned int throttlePercentage = map(pinValueConstrained, throttlePinMin, throttlePinMax, 0, 100);

  if (throttlePercentage < 5) {
    return 0;
  }

  if (throttlePercentage > 95) {
    return 100;
  }

  return throttlePercentage;
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
    buzzer.beepWarning();
    armingTries++;

    if (armingTries > 2) {
      armingTries = 0;
      resetCalibration();
    }

    return;
  }

  throttleArmed = true;
  buzzer.beepSuccess();
  if (canbus.isReady()) {
    canbus.setLedColor(Canbus::ledColorGreen);
  }
}

void Throttle::setDisarmed()
{
  throttleArmed = false;
  cancelCruise();
  buzzer.beepError();
  if (canbus.isReady()) {
    canbus.setLedColor(Canbus::ledColorRed);
  }
}

void Throttle::cancelCruise()
{
  cruising = false;
  cruisingThrottlePosition = 0;
  lastThrottlePosition = 0;
  timeThrottlePosition = 0;
}
