// test/BmsCapacitySyncLogicTest.cpp
// Host-only unit test for the BMS capacity sync decision. Compile with:
//   c++ -std=c++17 test/BmsCapacitySyncLogicTest.cpp -o "${TMPDIR:-/tmp}/bms_cap_sync_test"
#include <cassert>
#include <cstdint>
#include <iostream>
#include "../src/BmsCapacitySync/BmsCapacitySyncLogic.h"
using namespace std;

static BmsCapacitySyncInput in(uint32_t now, uint32_t bms, uint32_t adopted, bool armed = false) {
    BmsCapacitySyncInput i;
    i.nowMs          = now;
    i.bmsCapacityMah = bms;
    i.adoptedMah     = adopted;
    i.armed          = armed;
    return i;
}

static bool isNone(const BmsCapacitySyncDecision& d)  { return d.action == BmsCapacitySyncAction::None; }
static bool isApply(const BmsCapacitySyncDecision& d, uint32_t mah) {
    return d.action == BmsCapacitySyncAction::Apply && d.mah == mah;
}
static bool isOutOfRange(const BmsCapacitySyncDecision& d, uint32_t mah) {
    return d.action == BmsCapacitySyncAction::OutOfRange && d.mah == mah;
}

void test_no_bms_value_does_nothing() {
    BmsCapacitySyncLogic l;
    for (uint32_t t = 0; t <= 10000; t += 500) {
        assert(isNone(l.update(in(t, 0, 18000))));
    }
    cout << "test_no_bms_value_does_nothing passed\n";
}

void test_applies_after_stable_window() {
    BmsCapacitySyncLogic l;
    assert(isNone(l.update(in(1000, 34000, 18000))));
    assert(isNone(l.update(in(3999, 34000, 18000))));   // 2999 ms
    assert(isApply(l.update(in(4000, 34000, 18000)), 34000));  // 3000 ms
    cout << "test_applies_after_stable_window passed\n";
}

void test_equal_to_adopted_never_applies() {
    BmsCapacitySyncLogic l;
    for (uint32_t t = 0; t <= 10000; t += 500) {
        assert(isNone(l.update(in(t, 18000, 18000))));
    }
    cout << "test_equal_to_adopted_never_applies passed\n";
}

void test_change_restarts_window() {
    BmsCapacitySyncLogic l;
    l.update(in(0, 34000, 18000));
    assert(isNone(l.update(in(2500, 20000, 18000))));   // new candidate at 2500
    assert(isNone(l.update(in(5499, 20000, 18000))));
    assert(isApply(l.update(in(5500, 20000, 18000)), 20000));
    cout << "test_change_restarts_window passed\n";
}

void test_alternating_values_never_apply() {
    BmsCapacitySyncLogic l;
    for (uint32_t t = 0; t <= 20000; t += 1000) {
        const uint32_t v = (t / 1000) % 2 ? 34000 : 52000;
        assert(isNone(l.update(in(t, v, 18000))));
    }
    cout << "test_alternating_values_never_apply passed\n";
}

void test_dropout_restarts_window() {
    BmsCapacitySyncLogic l;
    l.update(in(0, 34000, 18000));
    assert(isNone(l.update(in(2000, 0, 18000))));        // BMS gone
    assert(isNone(l.update(in(2100, 34000, 18000))));    // back, window restarts at 2100
    assert(isNone(l.update(in(5099, 34000, 18000))));
    assert(isApply(l.update(in(5100, 34000, 18000)), 34000));
    cout << "test_dropout_restarts_window passed\n";
}

void test_pending_while_armed_applies_on_disarm() {
    BmsCapacitySyncLogic l;
    l.update(in(0, 34000, 18000, true));
    for (uint32_t t = 1000; t <= 600000; t += 1000) {
        assert(isNone(l.update(in(t, 34000, 18000, true))));
    }
    // First disarmed tick: no fresh 3 s wait.
    assert(isApply(l.update(in(600001, 34000, 18000, false)), 34000));
    cout << "test_pending_while_armed_applies_on_disarm passed\n";
}

void test_out_of_range_reported_once_per_value() {
    BmsCapacitySyncLogic l;
    l.update(in(0, 100000, 18000));
    assert(isOutOfRange(l.update(in(3000, 100000, 18000)), 100000));
    assert(isNone(l.update(in(4000, 100000, 18000))));
    assert(isNone(l.update(in(60000, 100000, 18000))));
    // A different out-of-range value reports again.
    l.update(in(61000, 999, 18000));
    assert(isOutOfRange(l.update(in(64000, 999, 18000)), 999));
    assert(isNone(l.update(in(65000, 999, 18000))));
    cout << "test_out_of_range_reported_once_per_value passed\n";
}

void test_range_edges() {
    {
        BmsCapacitySyncLogic l;
        l.update(in(0, 1000, 18000));
        assert(isApply(l.update(in(3000, 1000, 18000)), 1000));
    }
    {
        BmsCapacitySyncLogic l;
        l.update(in(0, 65000, 18000));
        assert(isApply(l.update(in(3000, 65000, 18000)), 65000));
    }
    {
        BmsCapacitySyncLogic l;
        l.update(in(0, 999, 18000));
        assert(isOutOfRange(l.update(in(3000, 999, 18000)), 999));
    }
    {
        BmsCapacitySyncLogic l;
        l.update(in(0, 65001, 18000));
        assert(isOutOfRange(l.update(in(3000, 65001, 18000)), 65001));
    }
    cout << "test_range_edges passed\n";
}

void test_after_apply_settles() {
    // The wrapper hands the value to BatteryMonitor; next tick adopted == candidate.
    BmsCapacitySyncLogic l;
    l.update(in(0, 34000, 18000));
    assert(isApply(l.update(in(3000, 34000, 18000)), 34000));
    assert(isNone(l.update(in(3001, 34000, 34000))));
    assert(isNone(l.update(in(9000, 34000, 34000))));
    cout << "test_after_apply_settles passed\n";
}

void test_first_value_applies_even_if_equal_to_configured() {
    // Nothing adopted yet (0): a BMS value equal to the configured capacity
    // is still adopted, so a later CFG_SET cannot take the count over.
    BmsCapacitySyncLogic l;
    l.update(in(0, 18000, 0));
    assert(isApply(l.update(in(3000, 18000, 0)), 18000));
    cout << "test_first_value_applies_even_if_equal_to_configured passed\n";
}

void test_new_bms_value_reapplies() {
    // A different stable value from the BMS replaces the adopted one.
    BmsCapacitySyncLogic l;
    l.update(in(0, 20000, 34000));
    assert(isNone(l.update(in(2999, 20000, 34000))));
    assert(isApply(l.update(in(3000, 20000, 34000)), 20000));
    cout << "test_new_bms_value_reapplies passed\n";
}

void test_millis_wrap_across_window() {
    BmsCapacitySyncLogic l;
    const uint32_t start = 0xFFFFFFFFu - 999u;          // 1000 ms before wrap
    l.update(in(start, 34000, 18000));
    assert(isNone(l.update(in(1999, 34000, 18000))));   // 2999 ms elapsed
    assert(isApply(l.update(in(2000, 34000, 18000)), 34000));
    cout << "test_millis_wrap_across_window passed\n";
}

int main() {
    test_no_bms_value_does_nothing();
    test_applies_after_stable_window();
    test_equal_to_adopted_never_applies();
    test_change_restarts_window();
    test_alternating_values_never_apply();
    test_dropout_restarts_window();
    test_pending_while_armed_applies_on_disarm();
    test_out_of_range_reported_once_per_value();
    test_range_edges();
    test_after_apply_settles();
    test_first_value_applies_even_if_equal_to_configured();
    test_new_bms_value_reapplies();
    test_millis_wrap_across_window();
    cout << "BmsCapacitySyncLogicTest: all passed\n";
    return 0;
}
