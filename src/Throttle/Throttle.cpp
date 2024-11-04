#include <Arduino.h>
#include "Throttle.h"

#include "../config.h"

Throttle::Throttle() {
  memset(
    &throttlePinValues, 
    0,
    sizeof(throttlePinValues[0]) * SAMPLES_FOR_FILTER
  );

  throttlePinValueFiltered = 0;
  lastThrottleRead = 0;

  throttleArmed = false;

  cruising = false;
  cruisingThrottlePosition = 0;
  lastThrottlePosition = 0;
  timeThrottlePosition = 0;
}

void Throttle::handle()
{
  unsigned long now = millis();

  if (now - lastThrottleRead < 10) {
    return;
  }

  lastThrottleRead = now;
  readThrottlePin();

  #if ENABLED_CRUISE_CONTROL
  checkIfChangedCruiseState();
  #endif
}

void Throttle::readThrottlePin()
{
  memcpy(
    &throttlePinValues[1], 
    &throttlePinValues, 
    sizeof(throttlePinValues[0]) * (SAMPLES_FOR_FILTER - 1)
  );

  throttlePinValues[SAMPLES_FOR_FILTER - 1] = analogRead(THROTTLE_PIN);

  int sum = 0;
  for (int i = 0; i < SAMPLES_FOR_FILTER; i++) {
    sum += throttlePinValues[i];
  }

  throttlePinValueFiltered = sum / SAMPLES_FOR_FILTER;
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
      cruising = true;
      cruisingThrottlePosition = throttlePercentage;

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
    cruising = false;
    cruisingThrottlePosition = 0;
    lastThrottlePosition = 0;

    return;
  }
}

unsigned int Throttle::getThrottlePercentage()
{
  unsigned int throttlePercentage = map(throttlePinValueFiltered, THROTTLE_PIN_MIN, THROTTLE_PIN_MAX, 0, 100);

  if (throttlePercentage < 5) {
    return 0;
  }

  if (throttlePercentage > 95) {
    return 100;
  }

  return throttlePercentage;
} 
