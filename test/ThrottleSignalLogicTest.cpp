// test/ThrottleSignalLogicTest.cpp
#include <cassert>
#include <iostream>
#include "../src/Throttle/ThrottleSignalLogic.h"
using namespace std;

// Wired: zero tolerance — any invalid sample disarms on the same tick.
static const ThrottleSignalConfig kWired{0, 0, 0};
// Wireless: matches the old RemoteLinkLogic timings, plus a 200 ms recovery
// guard so a flapping link cannot look "recovered" mid-episode.
static const ThrottleSignalConfig kWireless{500, 3000, 200};

void test_valid_stays_ok() {
    ThrottleSignalLogic logic;
    assert(logic.update(true, 0, kWired) == ThrottleSignalAction::Ok);
    assert(logic.update(true, 1000000, kWireless) == ThrottleSignalAction::Ok);
    cout << "PASS: valid reading always stays Ok\n";
}

void test_wired_disarms_on_first_invalid_sample() {
    ThrottleSignalLogic logic;
    assert(logic.update(true, 0, kWired) == ThrottleSignalAction::Ok);
    assert(logic.update(false, 10, kWired) == ThrottleSignalAction::Disarm);
    cout << "PASS: wired disarms immediately, no tolerance\n";
}

void test_wireless_debounce_then_forcezero() {
    ThrottleSignalLogic logic;
    assert(logic.update(false, 0, kWireless) == ThrottleSignalAction::Ok);
    assert(logic.update(false, 499, kWireless) == ThrottleSignalAction::Ok);
    assert(logic.update(false, 500, kWireless) == ThrottleSignalAction::ForceZero);
    cout << "PASS: wireless tolerates up to the 500ms debounce before ForceZero\n";
}

void test_wireless_disarms_after_3s() {
    ThrottleSignalLogic logic;
    logic.update(false, 0, kWireless);
    assert(logic.update(false, 2999, kWireless) == ThrottleSignalAction::ForceZero);
    assert(logic.update(false, 3000, kWireless) == ThrottleSignalAction::Disarm);
    cout << "PASS: wireless disarms at the 3s mark\n";
}

void test_wireless_flapping_link_does_not_reset_the_clock() {
    ThrottleSignalLogic logic;
    logic.update(false, 0, kWireless);                                     // episode starts at t=0
    assert(logic.update(false, 500, kWireless) == ThrottleSignalAction::ForceZero);
    assert(logic.update(true,  600, kWireless) == ThrottleSignalAction::ForceZero);  // one good packet — not recovered yet (only 0ms sustained)
    assert(logic.update(false, 650, kWireless) == ThrottleSignalAction::ForceZero);  // back to bad before recovering
    // No further valid samples: episode clock has been running since t=0 the
    // whole time, unaffected by the single blip at t=600.
    assert(logic.update(false, 3000, kWireless) == ThrottleSignalAction::Disarm);
    cout << "PASS: a single-tick blip does not reset the invalid-episode clock\n";
}

void test_wireless_recovers_after_sustained_validity() {
    ThrottleSignalLogic logic;
    logic.update(false, 0, kWireless);
    assert(logic.update(false, 500, kWireless) == ThrottleSignalAction::ForceZero);
    // Valid from t=501 onward, sustained for >= 200ms (recoveryMs).
    assert(logic.update(true, 501, kWireless) == ThrottleSignalAction::ForceZero); // 0ms sustained
    assert(logic.update(true, 700, kWireless) == ThrottleSignalAction::ForceZero); // 199ms sustained
    assert(logic.update(true, 701, kWireless) == ThrottleSignalAction::Ok);        // 200ms sustained — recovered
    cout << "PASS: recovers to Ok after 200ms of sustained validity\n";
}

void test_recovery_actually_clears_the_episode() {
    ThrottleSignalLogic logic;
    logic.update(false, 0, kWireless);
    logic.update(false, 500, kWireless);
    logic.update(true, 501, kWireless);
    logic.update(true, 701, kWireless); // recovered, per the previous test
    // A brand new invalid episode must restart from Ok, not resume mid-way.
    assert(logic.update(false, 750, kWireless) == ThrottleSignalAction::Ok);
    assert(logic.update(false, 1249, kWireless) == ThrottleSignalAction::Ok);
    assert(logic.update(false, 1250, kWireless) == ThrottleSignalAction::ForceZero);
    cout << "PASS: recovery fully resets the episode clock for the next fault\n";
}

void test_wireless_never_disarms_a_currently_valid_sample() {
    ThrottleSignalLogic logic;
    logic.update(false, 0, kWireless); // episode starts
    // Valid continuously from t=2850, recovery (200ms) not yet met at t=3000
    // even though elapsed-since-first-invalid crosses disarmMs (3000) there.
    assert(logic.update(true, 2850, kWireless) == ThrottleSignalAction::ForceZero);
    assert(logic.update(true, 2950, kWireless) == ThrottleSignalAction::ForceZero);
    assert(logic.update(true, 3000, kWireless) != ThrottleSignalAction::Disarm);
    assert(logic.update(true, 3050, kWireless) == ThrottleSignalAction::Ok); // recovered at 200ms sustained
    cout << "PASS: never disarms while the current sample is valid\n";
}

void test_reset_clears_state() {
    ThrottleSignalLogic logic;
    logic.update(false, 0, kWired); // Disarm
    logic.reset();
    assert(logic.update(true, 1, kWired) == ThrottleSignalAction::Ok);
    cout << "PASS: reset() clears any in-progress episode\n";
}

int main() {
    test_valid_stays_ok();
    test_wired_disarms_on_first_invalid_sample();
    test_wireless_debounce_then_forcezero();
    test_wireless_disarms_after_3s();
    test_wireless_flapping_link_does_not_reset_the_clock();
    test_wireless_recovers_after_sustained_validity();
    test_recovery_actually_clears_the_episode();
    test_wireless_never_disarms_a_currently_valid_sample();
    test_reset_clears_state();
    cout << "ThrottleSignalLogicTest: all passed" << endl;
    return 0;
}
