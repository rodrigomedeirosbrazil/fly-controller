#include <Arduino.h>
#include "Throttle.h"

#include "../config.h"

namespace {
constexpr ThrottleSignalConfig kWiredSignalConfig{0, 0, 0};
}

Throttle::Throttle(ReadFn readFn, ReadOkFn readOkFn) : readFn(readFn), readOkFn(readOkFn) {
  memset(
    &pinValues,
    0,
    sizeof(pinValues[0]) * samples
  );

  pinValueFiltered = 0;
  lastThrottleRead = 0;

  throttleArmed = false;
  lastSampleOk = true;
  lastDisarmReason = DisarmReason::None;
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
  engagement.update(pinValueFiltered, throttlePinMin, throttlePinMax);

  // Handle calibration if not yet calibrated
  if (!calibrated) {
    handleCalibration(now);
    return; // band isn't established yet — signal-validity checks need it
  }

  updateSignalValidity(now);
}

void Throttle::updateSignalValidity(unsigned long now)
{
  // Wired validity: band-clamped calibrated range plus I2C-fault
  // debouncing — see ThrottleWiredValidity.h for the full rationale.
  bool valid = wiredValidity.isValid(pinValueFiltered, throttlePinMin, throttlePinMax, ADC_MAX_VALUE);

  ThrottleSignalConfig cfg = kWiredSignalConfig;

  ThrottleSignalAction action = signalLogic.update(valid, now, cfg);

  switch (action) {
    case ThrottleSignalAction::Ok:
      break;
    case ThrottleSignalAction::ForceZero:
      // Unreachable with the wired {0,0,0} config (disarmMs = 0 never ramps).
      // The wireless remote used to feed zeros here; it was removed 2026-11-17.
      break;
    case ThrottleSignalAction::Disarm:
      setDisarmed(DisarmReason::ThrottleWiredInvalid);
      break;
  }
}

void Throttle::resetCalibration()
{
  calibrated = false;
  calibrationLogic.reset();
  engagement.reset();
  signalLogic.reset();
  wiredValidity.reset();
}

void Throttle::handleCalibration(unsigned long now)
{
  switch (calibrationLogic.update(pinValueFiltered, (uint32_t)now)) {
    case CalibrationEvent::MaxCaptured:
      sound.play(SoundEvent::CalibrationStep);
      break;
    case CalibrationEvent::Complete:
      throttlePinMin = calibrationLogic.pinMin();
      throttlePinMax = calibrationLogic.pinMax();
      calibrated = true;
      sound.play(SoundEvent::CalibrationComplete);
      break;
    case CalibrationEvent::RestartWithBeep:
      sound.play(SoundEvent::ArmingBlocked);
      break;
    case CalibrationEvent::Restart:  // silent: the throttle never moved
    case CalibrationEvent::None:
      break;
  }
}

void Throttle::readThrottlePin()
{
  memmove(
    &pinValues,
    &pinValues[1],
    sizeof(pinValues[0]) * (samples - 1)
  );

  // Always poll the sensor so lastSampleOk stays live; the real reading is
  // fed unconditionally (the output stage ignores demand once disarmed and
  // ramps the motor down at its decel rate).
  pinValues[samples - 1] = readFn();
  // lastReadOk() is tracked per ADS1115 channel (see ADS1115.h), so this
  // only needs to observe the same channel readFn() just read — no ordering
  // dependency on other components reading other channels.
  lastSampleOk = readOkFn();
  wiredValidity.recordSample(lastSampleOk);

  // Calculate moving average
  int sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += pinValues[i];
  }

  pinValueFiltered = sum / samples;
}

uint32_t Throttle::getDemandPpm()
{
  if (!calibrated) {
    return 0;
  }

  return throttleDemandPpm(pinValueFiltered, throttlePinMin, throttlePinMax, engagement.isEngaged());
}

unsigned int Throttle::getThrottlePercentage()
{
  return demandPercent(getDemandPpm());
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
  if (!calibrated) {
    sound.play(SoundEvent::ArmingBlocked);  // Warning: must calibrate throttle first
    return;
  }

  if (engagement.isEngaged()) {
    sound.play(SoundEvent::ArmingBlocked);
    armingTries++;

    if (armingTries > 2) {
      armingTries = 0;
      resetCalibration();
    }

    return;
  }

  signalLogic.reset();
  lastDisarmReason = DisarmReason::None;
  wiredValidity.reset();
  throttleArmed = true;
  power.onArmed();
}

void Throttle::setDisarmed(DisarmReason reason)
{
  if (!throttleArmed) {
    return;
  }

  throttleArmed = false;
  lastDisarmReason = reason;

  // Only the manual disarm gets an event. A fault disarm is announced by
  // SoundState::FaultDisarm, declared from lastDisarmReason in main.cpp's
  // updateSoundState() -- it has to persist until the pilot re-arms, and an
  // event cannot be stopped once started.
  if (reason == DisarmReason::Manual) {
    sound.play(SoundEvent::Disarmed);
  }
}
