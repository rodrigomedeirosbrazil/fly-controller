#include <Arduino.h>
#include "Throttle.h"

#include "../config.h"

Throttle::Throttle() {
  pinValueFiltered = 0;
  sampleCount = 0;
  lastSampleTimeMicros = 0;
  memset(
    &filterSamples,
    0,
    sizeof(filterSamples[0]) * FILTER_SAMPLE_COUNT
  );

  throttleArmed = false;
  resetCalibration();

  throttlePinMin = 0;
  throttlePinMax = 0;

  armingTries = 0;
}

void Throttle::handle()
{
  unsigned long now_us = micros();

  // Collect one sample at a time without blocking
  if (sampleCount < FILTER_SAMPLE_COUNT && now_us - lastSampleTimeMicros >= SAMPLE_INTERVAL_US) {
    lastSampleTimeMicros = now_us;
    filterSamples[sampleCount] = analogRead(THROTTLE_PIN);
    sampleCount++;
  }

  // When enough samples are collected, process them to get a new filtered value
  if (sampleCount >= FILTER_SAMPLE_COUNT) {
    // Simple bubble sort for small N
    for (int i = 0; i < FILTER_SAMPLE_COUNT - 1; i++) {
      for (int j = i + 1; j < FILTER_SAMPLE_COUNT; j++) {
        if (filterSamples[j] < filterSamples[i]) {
          int temp = filterSamples[i];
          filterSamples[i] = filterSamples[j];
          filterSamples[j] = temp;
        }
      }
    }

    // Discard the two lowest and two highest readings and average the rest
    long sum = 0;
    for (int i = 2; i < FILTER_SAMPLE_COUNT - 2; i++) {
      sum += filterSamples[i];
    }
    pinValueFiltered = sum / (FILTER_SAMPLE_COUNT - 4);

    // Reset for the next batch of samples
    sampleCount = 0;

    // Handle calibration only when a new filtered value is available
    if (!calibrated) {
      handleCalibration(millis());
    }
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

        buzzer.beepSuccess();

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
        buzzer.beepSuccess();

        // Set the min throttle value as the average
        throttlePinMin = calibrationSumMin / calibrationCountMin;

        // Calibration complete
        calibrated = true;
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
  if (hobbywing.isReady()) {
    hobbywing.setLedColor(Hobbywing::ledColorRed, Hobbywing::ledBlink5Hz);
  }
}

void Throttle::setDisarmed()
{
  throttleArmed = false;
  buzzer.beepError();
  if (hobbywing.isReady()) {
    hobbywing.setLedColor(Hobbywing::ledColorGreen);
  }
}
