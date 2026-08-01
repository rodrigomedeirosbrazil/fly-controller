#include <Arduino.h>
#include "Throttle.h"

#include "../config.h"

namespace {
constexpr ThrottleSignalConfig kWiredSignalConfig{0, 0, 0};
constexpr ThrottleSignalConfig kWirelessSignalConfig{500, 3000, 200};
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
  signalForcedZero = false;
  lastDisarmReason = DisarmReason::None;
  // `settings` may not be constructed yet at this point (global constructor
  // order across translation units is unspecified) — default to wired
  // rather than reading a not-yet-initialized object. If the real source is
  // wireless, the first updateSignalValidity() tick corrects this via one
  // harmless no-op reset().
  wasWireless = false;
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
  bool wireless = settings.getThrottleSource() == ThrottleSourceWireless;
  if (wireless != wasWireless) {
    // Source changed since the last tick — an in-progress invalid episode
    // was measured against the other source's tolerance and no longer means
    // anything. wiredValidity's I2C fail streak keeps accumulating while
    // switched to wireless (no ADC reads happen then), so it's equally
    // stale by the time a switch back to wired occurs — and wired has zero
    // tolerance, so a stale streak would disarm on the very first tick back.
    signalLogic.reset();
    wiredValidity.reset();
    wasWireless = wireless;
  }

  bool valid;

  if (wireless) {
    // Wireless validity is the raw link-freshness signal; the debounce/
    // disarm tolerance lives entirely in the wireless ThrottleSignalConfig
    // below, not in what counts as "valid" here.
    valid = lastSampleOk;
  } else {
    // Wired validity: band-clamped calibrated range plus I2C-fault
    // debouncing — see ThrottleWiredValidity.h for the full rationale.
    valid = wiredValidity.isValid(pinValueFiltered, throttlePinMin, throttlePinMax, ADC_MAX_VALUE);
  }

  ThrottleSignalConfig cfg = wireless ? kWirelessSignalConfig : kWiredSignalConfig;

  ThrottleSignalAction action = signalLogic.update(valid, now, cfg);

  switch (action) {
    case ThrottleSignalAction::Ok:
      signalForcedZero = false;
      break;
    case ThrottleSignalAction::ForceZero:
      signalForcedZero = true;
      break;
    case ThrottleSignalAction::Disarm:
      signalForcedZero = true;
      setDisarmed(wireless ? DisarmReason::ThrottleLinkLost : DisarmReason::ThrottleWiredInvalid);
      break;
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
  engagement.reset();
  signalLogic.reset();
  signalForcedZero = false;
  wiredValidity.reset();
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
  memmove(
    &pinValues,
    &pinValues[1],
    sizeof(pinValues[0]) * (samples - 1)
  );

  // Always call readFn() — even while forced to zero — so the underlying
  // sensor/link keeps being polled and lastSampleOk stays live. Only the
  // value fed into the moving average is overridden; this makes ForceZero
  // an invariant Throttle enforces unconditionally, not something each
  // ReadFn source has to remember to check itself.
  int rawValue = readFn();
  // ForceZero only ever needs to override the fed value for the wireless
  // source: it's the "ramp to zero while armed, signal invalid but not yet
  // disarmed" state, and the moving average must reflect that so motor
  // output ramps down smoothly. Wired's ForceZero is a single tick on its
  // way to an immediate Disarm (disarmMs=0) — poisoning pinValueFiltered
  // there would corrupt wiredValidity's own input (which reads
  // pinValueFiltered) and getThrottlePercentage()'s arm-blocking check,
  // creating a self-latching lockout: the moving average would never
  // recover from being fed zeros, so the wired band check could never
  // report "valid" again even after the real fault clears, and every
  // re-arm attempt would immediately re-disarm. Wired keeps feeding the
  // real reading unconditionally — nothing downstream needs it to be zero,
  // since the motor is already forced to ESC_MIN_PWM by handleEsc()'s
  // throttle.isArmed() check the instant it disarms.
  bool wireless = settings.getThrottleSource() == ThrottleSourceWireless;
  int oversampledValue = (wireless && signalForcedZero) ? 0 : rawValue;
  pinValues[samples - 1] = oversampledValue;
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
  if (!calibrated) {
    buzzer.beepArmingBlocked();  // Warning: must calibrate throttle first
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

  signalLogic.reset();
  signalForcedZero = false;
  lastDisarmReason = DisarmReason::None;
  wiredValidity.reset();
  throttleArmed = true;
}

void Throttle::setDisarmed(DisarmReason reason)
{
  if (!throttleArmed) {
    return;
  }

  throttleArmed = false;
  lastDisarmReason = reason;

  if (reason == DisarmReason::Manual) {
    buzzer.beepDisarmed();
  } else {
    buzzer.beepFaultDisarm();
  }
}
