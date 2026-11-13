// test/BmsAlertLogicTest.cpp
// Host-only unit test for BmsAlertLogic. Compile with:
//   c++ -std=c++17 test/BmsAlertLogicTest.cpp -o "${TMPDIR:-/tmp}/bms_alert_test"
#include <cassert>
#include <cstdint>
#include <iostream>
#include "../src/BmsAlert/BmsAlertLogic.h"
using namespace std;

static BmsAlertThresholds thr(uint16_t cutoff = 3150) {
    BmsAlertThresholds t;
    t.cellCutoffMv       = cutoff;
    t.lowCellDeltaGateMv = 100;
    t.maxDeltaMv         = 200;
    t.tempLimitC         = 55;
    return t;
}

static BmsAlertReading rd(uint16_t cellMin, uint16_t delta, int16_t temp,
                          bool cellAvail = true, bool tempAvail = true) {
    BmsAlertReading r;
    r.cellDataAvailable = cellAvail;
    r.cellMinMv         = cellMin;
    r.cellDeltaMv       = delta;
    r.tempAvailable     = tempAvail;
    r.tempMaxC          = temp;
    return r;
}

// Feeds a constant reading every 100 ms from `start` for `durMs` (inclusive of both ends).
static uint8_t feed(BmsAlertLogic& l, const BmsAlertReading& r, const BmsAlertThresholds& t,
                    uint32_t start, uint32_t durMs) {
    uint8_t m = 0;
    for (uint32_t d = 0; d <= durMs; d += 100) m = l.update(r, t, start + d);
    return m;
}

void test_per_cell_cutoff() { assert(perCellCutoffMv(44100, 14) == 3150); }
void test_per_cell_cutoff_zero_cells() { assert(perCellCutoffMv(44100, 0) == 0); }

void test_low_cell_needs_both_conditions() {
    BmsAlertLogic l;
    assert(!(feed(l, rd(3100, 80, 20), thr(), 0, 10000) & BmsAlertBit::LowCell));
    BmsAlertLogic l2;
    assert(!(feed(l2, rd(3200, 150, 20), thr(), 0, 10000) & BmsAlertBit::LowCell));
    BmsAlertLogic l3;
    uint8_t m = 0;
    for (uint32_t t = 0; t <= 2999; t += 100) m = l3.update(rd(3150, 101, 20), thr(), t);
    m = l3.update(rd(3150, 101, 20), thr(), 2999);
    assert(!(m & BmsAlertBit::LowCell));
    m = l3.update(rd(3150, 101, 20), thr(), 3000);
    assert(m & BmsAlertBit::LowCell);
}

void test_low_cell_disabled_without_cutoff() {
    BmsAlertLogic l;
    uint8_t m = feed(l, rd(2900, 300, 20), thr(0), 0, 10000);
    assert(!(m & BmsAlertBit::LowCell));
    assert(m & BmsAlertBit::HighDelta);
}

void test_low_cell_hysteresis() {
    {
        BmsAlertLogic l;
        assert(feed(l, rd(3100, 150, 20), thr(), 0, 4000) & BmsAlertBit::LowCell);
        assert(feed(l, rd(3190, 150, 20), thr(), 4100, 10000) & BmsAlertBit::LowCell);
        assert(!(feed(l, rd(3200, 150, 20), thr(), 14100, 3000) & BmsAlertBit::LowCell));
    }
    {
        BmsAlertLogic l;
        assert(feed(l, rd(3100, 150, 20), thr(), 0, 4000) & BmsAlertBit::LowCell);
        assert(feed(l, rd(3100, 95, 20), thr(), 4100, 10000) & BmsAlertBit::LowCell);
        assert(!(feed(l, rd(3100, 89, 20), thr(), 14100, 3000) & BmsAlertBit::LowCell));
    }
}

void test_high_delta_enter_and_exit() {
    BmsAlertLogic l;
    assert(!(feed(l, rd(3300, 200, 20), thr(), 0, 10000) & BmsAlertBit::HighDelta));
    assert(feed(l, rd(3300, 201, 20), thr(), 10100, 3000) & BmsAlertBit::HighDelta);
    assert(feed(l, rd(3300, 195, 20), thr(), 13200, 10000) & BmsAlertBit::HighDelta);
    assert(!(feed(l, rd(3300, 189, 20), thr(), 23300, 3000) & BmsAlertBit::HighDelta));
}

void test_high_temp_enter_and_exit() {
    BmsAlertLogic l;
    assert(!(feed(l, rd(3300, 10, 54), thr(), 0, 10000) & BmsAlertBit::HighTemp));
    assert(feed(l, rd(3300, 10, 55), thr(), 10100, 3000) & BmsAlertBit::HighTemp);
    assert(feed(l, rd(3300, 10, 53), thr(), 13200, 10000) & BmsAlertBit::HighTemp);
    assert(!(feed(l, rd(3300, 10, 51), thr(), 23300, 3000) & BmsAlertBit::HighTemp));
}

void test_flapping_never_raises() {
    BmsAlertLogic l;
    for (uint32_t t = 0; t <= 20000; t += 1000) {
        uint16_t delta = ((t / 1000) % 2 == 0) ? 250 : 150;
        assert(!(l.update(rd(3300, delta, 20), thr(), t) & BmsAlertBit::HighDelta));
    }
}

void test_unavailable_clears_immediately() {
    BmsAlertLogic l;
    assert(feed(l, rd(3300, 250, 60), thr(), 0, 4000) == (BmsAlertBit::HighDelta | BmsAlertBit::HighTemp));
    uint8_t m = l.update(rd(3300, 250, 60, false, true), thr(), 4100);
    assert(m == BmsAlertBit::HighTemp);
    m = l.update(rd(3300, 250, 60, false, false), thr(), 4200);
    assert(m == 0);
}

void test_unavailable_resets_timer() {
    BmsAlertLogic l;
    for (uint32_t t = 0; t <= 2000; t += 100) l.update(rd(3300, 250, 20), thr(), t);
    l.update(rd(3300, 250, 20, false), thr(), 2100);
    assert(!(l.update(rd(3300, 250, 20), thr(), 2200) & BmsAlertBit::HighDelta));
    assert(!(l.update(rd(3300, 250, 20), thr(), 5199) & BmsAlertBit::HighDelta));
    assert(l.update(rd(3300, 250, 20), thr(), 5200) & BmsAlertBit::HighDelta);
}

void test_alerts_are_independent() {
    BmsAlertLogic l;
    uint8_t m = feed(l, rd(3100, 250, 60), thr(), 0, 3000);
    assert(m == (BmsAlertBit::LowCell | BmsAlertBit::HighDelta | BmsAlertBit::HighTemp));
    assert(l.getMask() == m);
}

void test_debounce_across_millis_rollover() {
    BmsAlertLogic l;
    const uint32_t start = 0xFFFFF000u;
    l.update(rd(3300, 10, 60), thr(), start);
    assert(!(l.update(rd(3300, 10, 60), thr(), start + 2999) & BmsAlertBit::HighTemp));
    assert(l.update(rd(3300, 10, 60), thr(), start + 3000) & BmsAlertBit::HighTemp);
}

int main() {
    test_per_cell_cutoff();
    test_per_cell_cutoff_zero_cells();
    test_low_cell_needs_both_conditions();
    test_low_cell_disabled_without_cutoff();
    test_low_cell_hysteresis();
    test_high_delta_enter_and_exit();
    test_high_temp_enter_and_exit();
    test_flapping_never_raises();
    test_unavailable_clears_immediately();
    test_unavailable_resets_timer();
    test_alerts_are_independent();
    test_debounce_across_millis_rollover();
    cout << "BmsAlertLogicTest: all tests passed" << endl;
    return 0;
}
