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
  limited = false;
  thresholdToLimit = 0;
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

void Throttle::setCruising(int throttlePosition)
{
  cruisingThrottlePosition = throttlePosition;
  cruising = true;
}

unsigned int Throttle::getThrottlePosition()
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

unsigned int Throttle::getThrottle()
{
  if (!isArmed()) {
    return 0;
  }

  if (isCruising()) {
    return cruisingThrottlePosition;
  }

  if (isLimited()) {
    return handleLimited();
  }

  return getThrottlePosition();
}

void Throttle::setArmed()
{
  if (throttleArmed) {
    return;
  }

  if (getThrottlePosition() > 0) {
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
  setLimiting(cruisingThrottlePosition, 5);
  cruisingThrottlePosition = 0;
}

void Throttle::setLimiting(unsigned int throttlePosition, unsigned int threshold) {
  limited = true;
  lastThrottlePosition = throttlePosition;
  thresholdToLimit = threshold;
}

unsigned int Throttle::handleLimited() {
  if (millis() - lastThrottleRead < timeLimiting) {
    limited = false;
    return getThrottlePosition();
  }

  if (getThrottlePosition() - lastThrottlePosition >= thresholdToLimit) {  // accelerating too fast. limit
    unsigned int  limitedThrottle = lastThrottlePosition + thresholdToLimit;
    lastThrottlePosition = limitedThrottle;
    return limitedThrottle;
  }

  if (lastThrottlePosition - getThrottlePosition() >= thresholdToLimit * 2) {  // decelerating too fast. limit
    int limitedThrottle = lastThrottlePosition - thresholdToLimit * 2;  // double the decel vs accel
    lastThrottlePosition = limitedThrottle;
    return limitedThrottle;
  }

  lastThrottlePosition = getThrottlePosition();
  return getThrottlePosition();
}