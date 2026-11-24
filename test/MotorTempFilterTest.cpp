// test/MotorTempFilterTest.cpp
// Compile: c++ -std=c++17 test/MotorTempFilterTest.cpp -o "${TMPDIR:-/tmp}/motor_temp_filter_test" && "${TMPDIR:-/tmp}/motor_temp_filter_test"
#include <cassert>
#include <iostream>
#include "../src/Tmotor/MotorTempFilter.h"
using namespace std;

static const int32_t CEILING = 160000;

void test_first_sample_seeds() {
    MotorTempFilter f(CEILING);
    assert(f.addSample(30000, 0));
    assert(f.valueMilliCelsius() == 30000);
    assert(f.isFresh(0));
    cout << "PASS: first sample seeds\n";
}

void test_not_fresh_before_any_sample() {
    MotorTempFilter f(CEILING);
    assert(!f.isFresh(0));
    assert(f.valueMilliCelsius() == 0);
    cout << "PASS: not fresh before any sample\n";
}

void test_step_response_one_tau() {
    MotorTempFilter f(CEILING);
    f.addSample(100000, 0);
    for (uint32_t t = 100; t <= MOTOR_TEMP_FILTER_TAU_MS; t += 100) f.addSample(130000, t);
    int32_t v = f.valueMilliCelsius();
    assert(v >= 118000 && v <= 120000);
    cout << "PASS: step response is ~63% after one tau\n";
}

void test_step_response_rate_independent() {
    MotorTempFilter a(CEILING), b(CEILING);
    a.addSample(100000, 0);
    b.addSample(100000, 0);
    for (uint32_t t = 100; t <= 5000; t += 100) a.addSample(130000, t);
    for (uint32_t t = 500; t <= 5000; t += 500) b.addSample(130000, t);
    int32_t d = a.valueMilliCelsius() - b.valueMilliCelsius();
    assert(d > -1000 && d < 1000);
    cout << "PASS: step response is sample-rate independent\n";
}

void test_noise_attenuated() {
    MotorTempFilter f(CEILING);
    f.addSample(140000, 0);
    bool high = true;
    for (uint32_t t = 100; t <= 30000; t += 100) {
        f.addSample(high ? 145000 : 135000, t);
        high = !high;
        if (t > 10000) {
            int32_t v = f.valueMilliCelsius();
            assert(v >= 139000 && v <= 141000);
        }
    }
    cout << "PASS: alternating noise is attenuated\n";
}

void test_zero_dt_no_change() {
    MotorTempFilter f(CEILING);
    f.addSample(100000, 0);
    f.addSample(150000, 0);
    assert(f.valueMilliCelsius() == 100000);
    cout << "PASS: zero dt leaves the value unchanged\n";
}

void test_spike_above_ceiling_is_dropped() {
    MotorTempFilter f(CEILING);
    f.addSample(150000, 0);
    f.addSample(150000, 100);
    assert(!f.addSample(161000, 200));
    assert(f.valueMilliCelsius() == 150000);
    assert(f.isFresh(1050));
    cout << "PASS: a spike above the ceiling is dropped\n";
}

void test_ceiling_inclusive() {
    MotorTempFilter f(CEILING);
    assert(f.addSample(160000, 0));
    assert(!f.addSample(160001, 100));
    cout << "PASS: ceiling is inclusive\n";
}

void test_zero_is_plausible() {
    MotorTempFilter f(CEILING);
    assert(f.addSample(0, 0));
    assert(!f.addSample(-1, 100));
    cout << "PASS: zero is plausible, negative is not\n";
}

void test_stuck_out_of_range_goes_stale() {
    MotorTempFilter f(CEILING);
    f.addSample(150000, 0);
    for (uint32_t t = 100; t <= 2000; t += 100) f.addSample(170000, t);
    assert(f.isFresh(999));
    assert(!f.isFresh(1000));
    cout << "PASS: stuck out-of-range goes stale after 1 s\n";
}

void test_reseeds_after_gap() {
    MotorTempFilter f(CEILING);
    f.addSample(140000, 0);
    f.addSample(60000, 1000);
    assert(f.valueMilliCelsius() == 60000);
    cout << "PASS: re-seeds after a gap\n";
}

void test_no_reseed_inside_window() {
    MotorTempFilter f(CEILING);
    f.addSample(140000, 0);
    f.addSample(60000, 900);
    assert(f.valueMilliCelsius() > 60000 && f.valueMilliCelsius() < 140000);
    cout << "PASS: no re-seed inside the window\n";
}

void test_millis_rollover() {
    MotorTempFilter f(CEILING);
    f.addSample(100000, 0xFFFFFF00u);
    f.addSample(130000, 0x00000064u);
    assert(f.valueMilliCelsius() > 100000 && f.valueMilliCelsius() < 130000);
    assert(f.isFresh(0x00000064u));
    cout << "PASS: millis() rollover neither re-seeds nor goes stale\n";
}

void test_reset() {
    MotorTempFilter f(CEILING);
    f.addSample(100000, 0);
    f.addSample(110000, 100);
    f.reset();
    assert(!f.isFresh(100));
    assert(!f.isFresh(5000));
    assert(f.valueMilliCelsius() == 0);
    f.addSample(50000, 200);
    assert(f.valueMilliCelsius() == 50000);
    cout << "PASS: reset clears state and next sample seeds\n";
}

int main() {
    test_first_sample_seeds();
    test_not_fresh_before_any_sample();
    test_step_response_one_tau();
    test_step_response_rate_independent();
    test_noise_attenuated();
    test_zero_dt_no_change();
    test_spike_above_ceiling_is_dropped();
    test_ceiling_inclusive();
    test_zero_is_plausible();
    test_stuck_out_of_range_goes_stale();
    test_reseeds_after_gap();
    test_no_reseed_inside_window();
    test_millis_rollover();
    test_reset();
    cout << "MotorTempFilterTest: all passed" << endl;
    return 0;
}
