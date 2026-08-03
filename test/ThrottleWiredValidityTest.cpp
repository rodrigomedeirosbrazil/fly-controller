#include <cassert>
#include <iostream>
#include "../src/Throttle/ThrottleWiredValidity.h"
using namespace std;

static const int kAdcMax = 4095;

void test_midband_reading_is_valid_with_wide_calibration() {
    ThrottleWiredValidity v;
    // min=200, max=3800 — the exact repro calibration from the Critical bug.
    assert(v.isValid(200, 200, 3800, kAdcMax));
    assert(v.isValid(2000, 200, 3800, kAdcMax));
    assert(v.isValid(3800, 200, 3800, kAdcMax));
    assert(v.isValid(3900, 200, 3800, kAdcMax));
    cout << "PASS: mid-band and near-edge readings are valid with a wide calibration\n";
}

void test_open_circuit_reading_is_invalid_regardless_of_calibration() {
    ThrottleWiredValidity v;
    // Regression test for the Critical bug: with min=200, max=3800, the
    // unclamped low bound would have been -520, making a read of 0 "valid".
    assert(!v.isValid(0, 200, 3800, kAdcMax));
    cout << "PASS: an open-circuit reading of 0 is never valid, regardless of calibration\n";
}

void test_short_to_rail_reading_is_invalid_regardless_of_calibration() {
    ThrottleWiredValidity v;
    // Regression test for the Critical bug: with min=200, max=3800, the
    // unclamped high bound would have been 4160, making a read of 4095 "valid".
    assert(!v.isValid(kAdcMax, 200, 3800, kAdcMax));
    cout << "PASS: a short-to-rail reading of ADC_MAX_VALUE is never valid, regardless of calibration\n";
}

void test_full_span_calibration_still_clamps() {
    ThrottleWiredValidity v;
    // min=0, max=4095 — the widest possible calibration.
    assert(!v.isValid(0, 0, kAdcMax, kAdcMax));
    assert(!v.isValid(kAdcMax, 0, kAdcMax, kAdcMax));
    assert(v.isValid(2000, 0, kAdcMax, kAdcMax));
    cout << "PASS: full-span calibration still rejects both ADC rails\n";
}

void test_narrow_calibration_is_unaffected_by_the_clamp() {
    ThrottleWiredValidity v;
    // min=1000, max=1200, range=200: marginLow=40, marginHigh=20 — bounds
    // [960, 1220], nowhere near the ADC rails, so the clamp never fires and
    // the asymmetric margin is exactly as calibrated.
    assert(!v.isValid(959, 1000, 1200, kAdcMax));
    assert(v.isValid(960, 1000, 1200, kAdcMax));
    assert(v.isValid(1220, 1000, 1200, kAdcMax));
    assert(!v.isValid(1221, 1000, 1200, kAdcMax));
    cout << "PASS: narrow calibration keeps its exact asymmetric bounds, unclamped\n";
}

void test_i2c_streak_tolerates_up_to_two_consecutive_failures() {
    ThrottleWiredValidity v;
    v.recordSample(false);
    assert(v.isValid(2000, 200, 3800, kAdcMax)); // 1 failure — still valid
    v.recordSample(false);
    assert(v.isValid(2000, 200, 3800, kAdcMax)); // 2 failures — still valid
    cout << "PASS: up to 2 consecutive I2C failures don't affect validity\n";
}

void test_i2c_streak_of_three_invalidates_on_the_same_tick() {
    ThrottleWiredValidity v;
    v.recordSample(false);
    v.recordSample(false);
    v.recordSample(false);
    assert(!v.isValid(2000, 200, 3800, kAdcMax)); // 3rd consecutive failure — invalid immediately
    cout << "PASS: the 3rd consecutive I2C failure invalidates on the same tick\n";
}

void test_i2c_streak_resets_on_a_good_read() {
    ThrottleWiredValidity v;
    v.recordSample(false);
    v.recordSample(false);
    v.recordSample(true); // good read resets the streak
    v.recordSample(false);
    assert(v.isValid(2000, 200, 3800, kAdcMax)); // only 1 failure since the reset
    cout << "PASS: a good read resets the failure streak\n";
}

void test_reset_clears_the_streak() {
    ThrottleWiredValidity v;
    v.recordSample(false);
    v.recordSample(false);
    v.recordSample(false);
    assert(!v.isValid(2000, 200, 3800, kAdcMax));
    v.reset();
    assert(v.isValid(2000, 200, 3800, kAdcMax));
    cout << "PASS: reset() clears the failure streak\n";
}

int main() {
    test_midband_reading_is_valid_with_wide_calibration();
    test_open_circuit_reading_is_invalid_regardless_of_calibration();
    test_short_to_rail_reading_is_invalid_regardless_of_calibration();
    test_full_span_calibration_still_clamps();
    test_narrow_calibration_is_unaffected_by_the_clamp();
    test_i2c_streak_tolerates_up_to_two_consecutive_failures();
    test_i2c_streak_of_three_invalidates_on_the_same_tick();
    test_i2c_streak_resets_on_a_good_read();
    test_reset_clears_the_streak();
    cout << "ThrottleWiredValidityTest: all passed" << endl;
    return 0;
}
