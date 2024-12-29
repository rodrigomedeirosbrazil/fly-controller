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
  int pinValueConstrained = constrain(pinValueFiltered, THROTTLE_PIN_MIN, THROTTLE_PIN_MAX);
  unsigned int throttlePercentage = map(pinValueConstrained, THROTTLE_PIN_MIN, THROTTLE_PIN_MAX, 0, 100);

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

void Throttle::setDisarmed()
{
  throttleArmed = false;
  cancelCruise();
}

void Throttle::cancelCruise()
{
  cruising = false;
  cruisingThrottlePosition = 0;
  lastThrottlePosition = 0;
  timeThrottlePosition = 0;
}

void Throttle::setSmoothThrottleChange(unsigned int fromThrottlePosition, unsigned int toThrottlePosition)
{
  smoothThrottleChanging = true;
  timeStartSmoothThrottleChange = millis();
  lastThrottlePosition = fromThrottlePosition;
  targetThrottlePosition = toThrottlePosition;
}

void Throttle::cancelSmoothThrottleChange()
{
  smoothThrottleChanging = false;
}

unsigned int Throttle::getThrottlePercentageOnSmoothChange()
{
  if (!smoothThrottleChanging) {
    return 0;
  }

  unsigned long now = millis();
  unsigned long timeElapsed = now - timeStartSmoothThrottleChange;

  if (timeElapsed > smoothThrottleChangeSeconds * 1000) {
    cancelSmoothThrottleChange();
    return targetThrottlePosition;
  }

  unsigned int throttlePercentage = map(
    timeElapsed,
    0,
    smoothThrottleChangeSeconds * 1000,
    lastThrottlePosition,
    targetThrottlePosition
  );

  return throttlePercentage;
}