// test/TempProtectionAlertLogicTest.cpp
#include <cassert>
#include <iostream>
#include <vector>
#include "../src/TempProtectionAlert/TempProtectionAlertLogic.h"
using namespace std;

static const uint8_t MOTOR = 1 << 1;  // POWER_LIMIT_MOTOR_TEMP
static const uint8_t ESC   = 1 << 2;  // POWER_LIMIT_ESC_TEMP

typedef TempProtectionAlertLogic::Output Out;

static bool isLost(Out o)     { return o.playLost && !o.playRestored; }
static bool isRestored(Out o) { return o.playRestored && !o.playLost; }
static bool isSilent(Out o)   { return !o.playLost && !o.playRestored; }

// Runs armed with a constant mask from `from` to `to` (inclusive) in 100 ms
// steps and returns the times at which a lost alarm fired.
static vector<uint32_t> lostFires(TempProtectionAlertLogic& a, uint8_t mask, uint32_t from, uint32_t to) {
    vector<uint32_t> fires;
    for (uint32_t t = from; t <= to; t += 100) {
        Out o = a.update(true, mask, t);
        assert(!o.playRestored);
        if (o.playLost) fires.push_back(t);
    }
    return fires;
}

void test_silent_when_nothing_suspended() {
    TempProtectionAlertLogic a;
    for (uint32_t t = 0; t < 300000; t += 100) {
        assert(isSilent(a.update(true, 0, t)));
    }
    cout << "PASS: silent while every protection is active\n";
}

void test_silent_while_disarmed() {
    TempProtectionAlertLogic a;
    for (uint32_t t = 0; t < 300000; t += 100) {
        assert(isSilent(a.update(false, MOTOR | ESC, t)));
    }
    cout << "PASS: silent while disarmed, whatever is suspended\n";
}

void test_backoff_schedule() {
    TempProtectionAlertLogic a;
    vector<uint32_t> fires = lostFires(a, MOTOR, 0, 240000);
    vector<uint32_t> expected = {0, 10000, 30000, 60000, 120000, 180000, 240000};
    assert(fires == expected);
    cout << "PASS: alarms at 0, 10, 30, 60 s, then every 60 s\n";
}

void test_arming_with_protection_already_suspended_fires_at_once() {
    TempProtectionAlertLogic a;
    assert(isSilent(a.update(false, MOTOR, 0)));
    assert(isLost(a.update(true, MOTOR, 5000)));
    cout << "PASS: arming with a suspended protection alarms on the first armed tick\n";
}

void test_restored_fires_once_when_all_back() {
    TempProtectionAlertLogic a;
    assert(isLost(a.update(true, MOTOR, 0)));
    assert(isRestored(a.update(true, 0, 5000)));
    assert(isSilent(a.update(true, 0, 5100)));
    for (uint32_t t = 5200; t < 200000; t += 100) {
        assert(isSilent(a.update(true, 0, t)));
    }
    cout << "PASS: restored plays once and the reminders stop\n";
}

void test_second_protection_lost_restarts_schedule() {
    TempProtectionAlertLogic a;
    // Motor out from t=0; schedule well into its steady phase.
    vector<uint32_t> first = lostFires(a, MOTOR, 0, 70000);
    assert(first.back() == 60000);
    // ESC joins at 70 s: immediate alarm, then the schedule starts over.
    vector<uint32_t> second = lostFires(a, MOTOR | ESC, 70000, 100000);
    vector<uint32_t> expected = {70000, 80000, 100000};
    assert(second == expected);
    cout << "PASS: another protection joining alarms at once and restarts the backoff\n";
}

void test_partial_recovery_plays_nothing_and_keeps_reminding() {
    TempProtectionAlertLogic a;
    assert(isLost(a.update(true, MOTOR | ESC, 0)));
    // Motor comes back at 5 s, ESC still out: no "restored", reminder still due at 10 s.
    assert(isSilent(a.update(true, ESC, 5000)));
    assert(isSilent(a.update(true, ESC, 9900)));
    assert(isLost(a.update(true, ESC, 10000)));
    cout << "PASS: partial recovery is silent and reminders continue\n";
}

void test_no_restored_after_disarm() {
    TempProtectionAlertLogic a;
    assert(isLost(a.update(true, MOTOR, 0)));
    assert(isSilent(a.update(false, MOTOR, 1000)));
    // Recovers on the ground, then re-arms clean: nothing to announce.
    assert(isSilent(a.update(false, 0, 2000)));
    assert(isSilent(a.update(true, 0, 3000)));
    cout << "PASS: a recovery while disarmed is not announced on the next arm\n";
}

void test_rearm_restarts_schedule() {
    TempProtectionAlertLogic a;
    lostFires(a, MOTOR, 0, 70000);
    assert(isSilent(a.update(false, MOTOR, 71000)));
    vector<uint32_t> fires = lostFires(a, MOTOR, 80000, 110000);
    vector<uint32_t> expected = {80000, 90000, 110000};
    assert(fires == expected);
    cout << "PASS: re-arming restarts the backoff from the first alarm\n";
}

void test_millis_rollover() {
    TempProtectionAlertLogic a;
    const uint32_t nearMax = 0xFFFFFFFFu - 5000;
    assert(isLost(a.update(true, MOTOR, nearMax)));
    assert(isSilent(a.update(true, MOTOR, 4000)));   // 9001 ms elapsed
    assert(isLost(a.update(true, MOTOR, 5000)));     // 10001 ms elapsed
    cout << "PASS: reminder timing is correct across millis() rollover\n";
}

int main() {
    test_silent_when_nothing_suspended();
    test_silent_while_disarmed();
    test_backoff_schedule();
    test_arming_with_protection_already_suspended_fires_at_once();
    test_restored_fires_once_when_all_back();
    test_second_protection_lost_restarts_schedule();
    test_partial_recovery_plays_nothing_and_keeps_reminding();
    test_no_restored_after_disarm();
    test_rearm_restarts_schedule();
    test_millis_rollover();
    cout << "TempProtectionAlertLogicTest: all passed" << endl;
    return 0;
}
