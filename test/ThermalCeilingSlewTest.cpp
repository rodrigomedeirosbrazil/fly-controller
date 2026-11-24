#include <cassert>
#include <cstdint>
#include <iostream>
#include "../src/Power/ThermalCeilingSlew.h"
using namespace std;

void test_first_update_seeds() {
    ThermalCeilingSlew s;
    assert(s.update(40, 1000) == 40);
    cout << "PASS test_first_update_seeds" << endl;
}

void test_falls_at_10_pct_per_s() {
    ThermalCeilingSlew s;
    s.update(100, 0);
    assert(s.update(0, 500) == 95);
    assert(s.update(0, 1000) == 90);
    assert(s.update(0, 10000) == 0);
    cout << "PASS test_falls_at_10_pct_per_s" << endl;
}

void test_rises_at_2_pct_per_s() {
    ThermalCeilingSlew s;
    s.update(40, 0);
    assert(s.update(100, 500) == 41);
    assert(s.update(100, 5000) == 50);
    assert(s.update(100, 60000) == 100);
    cout << "PASS test_rises_at_2_pct_per_s" << endl;
}

void test_never_overshoots_target() {
    ThermalCeilingSlew s;
    s.update(50, 0);
    assert(s.update(48, 5000) == 48);
    assert(s.update(49, 10000) == 49);
    cout << "PASS test_never_overshoots_target" << endl;
}

void test_small_dt_accumulates() {
    ThermalCeilingSlew s;
    s.update(40, 0);
    unsigned v = 0;
    for (uint32_t t = 10; t <= 1000; t += 10) v = s.update(100, t);
    assert(v == 42);
    cout << "PASS test_small_dt_accumulates" << endl;
}

void test_release_then_recapture() {
    ThermalCeilingSlew s;
    s.update(40, 0);
    assert(s.update(100, 10000) == 60);
    assert(s.update(30, 11000) == 50);
    assert(s.update(30, 13000) == 30);
    cout << "PASS test_release_then_recapture" << endl;
}

void test_reset_reseeds() {
    ThermalCeilingSlew s;
    s.update(100, 0);
    s.update(0, 1000);
    s.reset();
    assert(s.update(70, 2000) == 70);
    cout << "PASS test_reset_reseeds" << endl;
}

void test_millis_rollover() {
    ThermalCeilingSlew s;
    s.update(100, 0xFFFFFE0C);  // 500 ms before the wrap
    assert(s.update(0, 0) == 95);
    cout << "PASS test_millis_rollover" << endl;
}

void test_target_above_100_clamped() {
    ThermalCeilingSlew s;
    assert(s.update(250, 0) == 100);
    cout << "PASS test_target_above_100_clamped" << endl;
}

int main() {
    test_first_update_seeds();
    test_falls_at_10_pct_per_s();
    test_rises_at_2_pct_per_s();
    test_never_overshoots_target();
    test_small_dt_accumulates();
    test_release_then_recapture();
    test_reset_reseeds();
    test_millis_rollover();
    test_target_above_100_clamped();
    cout << "ThermalCeilingSlewTest: all passed" << endl;
    return 0;
}
