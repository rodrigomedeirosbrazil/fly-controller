#include <Arduino.h>
#include "Throttle.h"

Throttle::Throttle(PwmReader *pwmReader) {
    this->pwmReader = pwmReader;
    throttleArmed = false;
    throttleFullReverseTime = 0;
    throttleFullReverseFirstTime = false;

    cruising = false;
    timeThrottlePosition = 0;
    lastThrottlePosition = 0;
    cruisingThrottlePosition = 0;
    cruisingStartTime = 0;

    for (unsigned int i = 0; i < MAX_SAMPLES; i++) {
      throttlePercentageSamples[i] = 0;
    };
}

void Throttle::tick()
{
  unsigned int now = millis();
  int throttlePercentage = pwmReader->getThrottlePercentage();

  checkIfChangedArmedState(throttlePercentage, now);
  
  #if ENABLED_CRUISE_CONTROL
  checkIfChangedCruiseState(
    isCruising() ? throttlePercentage : getThrottlePercentageFiltered(), 
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
  unsigned int delayToBeOnIdle = 2000;
  unsigned int throttleRange = 5;
  unsigned int minCrusingThrottle = 30;

  if (!cruising) {
    if (throttlePercentage > 0) {

      if (
        throttlePercentage > lastThrottlePosition + throttleRange
        || throttlePercentage < lastThrottlePosition - throttleRange
        || throttlePercentage < minCrusingThrottle
      ) {
        timeThrottlePosition = now;
        lastThrottlePosition = throttlePercentage;

        return;
      }

      if (
        now - timeThrottlePosition > delayToBeOnCruising
        && throttlePercentage > minCrusingThrottle
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

unsigned int Throttle::getThrottlePercentageFiltered()
{
  int throttlePercentage = pwmReader->getThrottlePercentage();

  for (unsigned int i = 1; i <= MAX_SAMPLES; i++) {
    throttlePercentageSamples[(i - 1)] = throttlePercentageSamples[i];
  };

  throttlePercentageSamples[MAX_SAMPLES] = throttlePercentage;

  unsigned int sum = 0;
  for (unsigned int i = 0; i < MAX_SAMPLES; i++) {
    sum += throttlePercentageSamples[i];
  }

  int averageThrottlePercentage = sum / MAX_SAMPLES;

  if (averageThrottlePercentage < 5) {
    return 0;
  }

  if (averageThrottlePercentage > 95) {
    return 100;
  }

  return averageThrottlePercentage;
}
