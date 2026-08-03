// src/ADS1115/SensorReadingValidity.h
#pragma once

// Pure sensor-reading validity check — no Arduino deps, host-testable.
// Reused by Temperature and BatteryVoltageSensor: both are ADS1115-backed
// sensors with a *fixed* physical valid range (unlike the throttle, whose
// valid range is calibration-relative — see ThrottleWiredValidity for that
// version).
//
// "Valid" requires two independent things, matching the throttle's own
// wired-validity philosophy (see ThrottleWiredValidity.h):
//   1. The reading falls inside [lowBound, highBound] (inclusive).
//   2. The I2C read hasn't failed 3 times in a row. This is a single
//      un-averaged bool per tick, unlike the reading itself (which each
//      caller typically averages over several samples) — a lone
//      conversion-timeout or bus NAK is a plausible transient, not proof of
//      a dead sensor, so it gets a small tolerance before it counts against
//      validity.
//
// The unit of `value`/`lowBound`/`highBound` is whatever domain the caller
// validates in (raw ADC counts for the NTC divider, millivolts for the
// battery divider) — this class doesn't care, it's a plain band check.
class SensorReadingValidity {
public:
    enum : unsigned int { I2C_FAIL_STREAK_THRESHOLD = 3 };

    SensorReadingValidity() : i2cFailStreak_(0) {}

    void recordSample(bool readOk) {
        if (readOk) {
            i2cFailStreak_ = 0;
        } else {
            i2cFailStreak_++; // unsigned wraparound is safe and irrelevant
                               // at realistic timescales
        }
    }

    void reset() { i2cFailStreak_ = 0; }

    bool isValid(int value, int lowBound, int highBound) const {
        bool inBand = value >= lowBound && value <= highBound;
        bool i2cOk = i2cFailStreak_ < I2C_FAIL_STREAK_THRESHOLD;
        return inBand && i2cOk;
    }

private:
    unsigned int i2cFailStreak_;
};
