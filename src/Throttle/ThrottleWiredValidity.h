#pragma once

// Pure wired-throttle validity decision — no Arduino deps, host-testable.
//
// "Valid" requires two independent things to both hold:
//   1. The filtered reading falls inside the calibrated band, widened by an
//      asymmetric margin (reading low only costs power; reading high is the
//      side that becomes unintended acceleration, so it gets a tighter
//      margin) — but the resulting bounds are clamped strictly inside
//      [1, adcMaxValue-1]. A wide calibrated span can make the percentage
//      margin larger than the distance to a physical ADC rail; without the
//      clamp, an open circuit (reads ~0) or a short to the rail (reads
//      ~adcMaxValue) could fall inside an unclamped bound and be classified
//      valid, depending on where a given unit happened to calibrate.
//   2. The I2C read hasn't failed I2C_FAIL_STREAK_THRESHOLD times in a row.
//      This is a single un-averaged bool per tick, unlike the filtered
//      reading (already protected by an 8-sample moving average) — a lone
//      conversion-timeout or bus NAK next to a running motor is a plausible
//      transient, not proof of a dead sensor, so it gets the same one-shot
//      tolerance the moving average already gives the analog value.
class ThrottleWiredValidity {
public:
    static const int MARGIN_LOW_PERCENT = 20;
    static const int MARGIN_HIGH_PERCENT = 10;
    static const unsigned int I2C_FAIL_STREAK_THRESHOLD = 3;

    ThrottleWiredValidity() : i2cFailStreak_(0) {}

    // Call once per read tick with this sample's I2C-read outcome.
    void recordSample(bool readOk) {
        if (readOk) {
            i2cFailStreak_ = 0;
        } else {
            i2cFailStreak_++; // unsigned wraparound is safe and irrelevant at
                               // realistic timescales (~57,500 days at 10ms/tick)
        }
    }

    void reset() { i2cFailStreak_ = 0; }

    bool isValid(int filtered, int calMin, int calMax, int adcMaxValue) const {
        int range = calMax - calMin;
        int marginLow  = (range * MARGIN_LOW_PERCENT) / 100;
        int marginHigh = (range * MARGIN_HIGH_PERCENT) / 100;
        int lowBound  = calMin - marginLow;
        int highBound = calMax + marginHigh;
        if (lowBound <= 0) lowBound = 1;
        if (highBound >= adcMaxValue) highBound = adcMaxValue - 1;
        bool inBand = filtered >= lowBound && filtered <= highBound;
        bool i2cOk = i2cFailStreak_ < I2C_FAIL_STREAK_THRESHOLD;
        return inBand && i2cOk;
    }

private:
    unsigned int i2cFailStreak_;
};
