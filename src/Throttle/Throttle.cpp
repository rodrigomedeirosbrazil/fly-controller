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
  unsigned int throttlePercentage = map(pinValueFiltered, THROTTLE_PIN_MIN, THROTTLE_PIN_MAX, 0, 100);

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
  if (throttleArmed) {
    return;
  }

  if (getThrottlePercentage() > 0) {
    return;
  }

  throttleArmed = true;
}
