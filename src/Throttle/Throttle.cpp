#include <Arduino.h>
#include "Throttle.h"

Throttle::Throttle() {
    throttleArmed = false;
    throttleFullReverseTime = 0;
    throttleFullReverseFirstTime = false;

    cruising = false;
    timeThrottlePosition = 0;
    lastThrottlePosition = 0;
    cruisingThrottlePosition = 0;
    cruisingStartTime = 0;
}

void Throttle::tick()
{
  unsigned int now = millis();
  int throttlePercentage = 0; // TODO: get it from hall sensor

  checkIfChangedArmedState(throttlePercentage, now);
  
  #if ENABLED_CRUISE_CONTROL
  checkIfChangedCruiseState(
    isCruising() ? throttlePercentage : getThrottlePercentage(), 
    now
  );
  #endif
}

void Throttle::checkIfChangedArmedState(int throttlePercentage, unsigned int now)
{
  unsigned int delayBetweenFullReverseAndArmed = 1000;

  if (!throttleArmed)
  {
    if (throttlePercentage < -90 && !throttleFullReverseFirstTime)
    {
      throttleFullReverseTime = now;
      return;
    }

    if (
        throttlePercentage > -90 && !throttleFullReverseFirstTime && (now - throttleFullReverseTime) < delayBetweenFullReverseAndArmed)
    {
      throttleFullReverseFirstTime = true;
      throttleFullReverseTime = now;
      return;
    }

    if (
        throttlePercentage < -90 && throttleFullReverseFirstTime && (now - throttleFullReverseTime) < delayBetweenFullReverseAndArmed)
    {
      throttleFullReverseFirstTime = true;
      throttleFullReverseTime = 0;
      throttleArmed = true;
      return;
    }

    if (now - throttleFullReverseTime > delayBetweenFullReverseAndArmed)
    {
      if (throttleFullReverseFirstTime)
      {
      }

      throttleFullReverseFirstTime = false;
      throttleFullReverseTime = 0;
      return;
    }
    return;
  }

  if (throttlePercentage < -90 && !throttleFullReverseFirstTime)
  {
    throttleFullReverseTime = now;
    return;
  }

  if (
      throttlePercentage > -90 && !throttleFullReverseFirstTime && (now - throttleFullReverseTime) < delayBetweenFullReverseAndArmed)
  {
    throttleFullReverseFirstTime = true;
    throttleFullReverseTime = now;
    return;
  }

  if (
      throttlePercentage < -90 && throttleFullReverseFirstTime && (now - throttleFullReverseTime) < delayBetweenFullReverseAndArmed)
  {
    throttleFullReverseFirstTime = true;
    throttleFullReverseTime = 0;
    throttleArmed = false;
    return;
  }

  if (now - throttleFullReverseTime > delayBetweenFullReverseAndArmed)
  {
    throttleFullReverseFirstTime = false;
    throttleFullReverseTime = 0;
    return;
  }
}

void Throttle::checkIfChangedCruiseState(int throttlePercentage, unsigned int now)
{
  if (! throttleArmed) {
    return;
  }

  unsigned int delayToBeOnCruising = 5000;
  unsigned int throttleRange = 5;
  unsigned int minCrusingThrottle = 30;

  if (!cruising) {
    if (throttlePercentage > 0) {

      if (
        throttlePercentage > lastThrottlePosition + (int) throttleRange
        || throttlePercentage < lastThrottlePosition - (int) throttleRange
        || throttlePercentage < (int) minCrusingThrottle
      ) {
        timeThrottlePosition = now;
        lastThrottlePosition = throttlePercentage;

        return;
      }

      if (
        now - timeThrottlePosition > delayToBeOnCruising
        && throttlePercentage > (int) minCrusingThrottle
      ) {
        cruising = true;
        cruisingThrottlePosition = throttlePercentage;
        cruisingStartTime = now;

        return;
      }
    }

    return;
  }

  if (
    throttlePercentage > lastThrottlePosition
    || throttlePercentage < -30
  ) {
    cruising = false;
    cruisingThrottlePosition = 0;
    cruisingStartTime = 0;

    return;
  }
}

unsigned int Throttle::getThrottlePercentage()
{
  int throttlePercentage = 0; // TODO: get it from hall sensor

  if (throttlePercentage < 5) {
    return 0;
  }

  if (throttlePercentage > 95) {
    return 100;
  }

  return throttlePercentage;
} 
