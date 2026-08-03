// test/SensorReadingValidityTest.cpp
#include <cassert>
#include <iostream>
#include "../src/ADS1115/SensorReadingValidity.h"
using namespace std;

void test_in_band_reading_is_valid() {
    SensorReadingValidity v;
    assert(v.isValid(1650, 91, 2954));  // 25C, well inside the NTC band
    cout << "PASS: in-band reading is valid\n";
}

void test_below_band_is_invalid() {
    SensorReadingValidity v;
    assert(!v.isValid(90, 91, 2954));
    cout << "PASS: one count below the band is invalid\n";
}

void test_above_band_is_invalid() {
    SensorReadingValidity v;
    assert(!v.isValid(2955, 91, 2954));
    cout << "PASS: one count above the band is invalid\n";
}

void test_band_edges_are_valid() {
    SensorReadingValidity v;
    assert(v.isValid(91, 91, 2954));
    assert(v.isValid(2954, 91, 2954));
    cout << "PASS: the band edges themselves are valid (inclusive)\n";
}

void test_i2c_streak_tolerates_up_to_two_consecutive_failures() {
    SensorReadingValidity v;
    v.recordSample(false);
    assert(v.isValid(1650, 91, 2954));
    v.recordSample(false);
    assert(v.isValid(1650, 91, 2954));
    cout << "PASS: up to 2 consecutive I2C failures don't affect validity\n";
}

void test_i2c_streak_of_three_invalidates_on_the_same_tick() {
    SensorReadingValidity v;
    v.recordSample(false);
    v.recordSample(false);
    v.recordSample(false);
    assert(!v.isValid(1650, 91, 2954));
    cout << "PASS: the 3rd consecutive I2C failure invalidates on the same tick\n";
}

void test_i2c_streak_resets_on_a_good_read() {
    SensorReadingValidity v;
    v.recordSample(false);
    v.recordSample(false);
    v.recordSample(true);
    v.recordSample(false);
    assert(v.isValid(1650, 91, 2954));
    cout << "PASS: a good read resets the failure streak\n";
}

void test_reset_clears_the_streak() {
    SensorReadingValidity v;
    v.recordSample(false);
    v.recordSample(false);
    v.recordSample(false);
    assert(!v.isValid(1650, 91, 2954));
    v.reset();
    assert(v.isValid(1650, 91, 2954));
    cout << "PASS: reset() clears the failure streak\n";
}

int main() {
    test_in_band_reading_is_valid();
    test_below_band_is_invalid();
    test_above_band_is_invalid();
    test_band_edges_are_valid();
    test_i2c_streak_tolerates_up_to_two_consecutive_failures();
    test_i2c_streak_of_three_invalidates_on_the_same_tick();
    test_i2c_streak_resets_on_a_good_read();
    test_reset_clears_the_streak();
    cout << "SensorReadingValidityTest: all passed" << endl;
    return 0;
}
