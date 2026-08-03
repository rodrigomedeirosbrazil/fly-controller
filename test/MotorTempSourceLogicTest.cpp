#include <cassert>
#include <iostream>
#include "../src/Tmotor/MotorTempSourceLogic.h"
using namespace std;

void test_can_wins_when_fresh_plausible_and_not_forced_to_ntc() {
    MotorTempReading r = selectMotorTempReading(false, true, 55000, 60.0, true);
    assert(r.milliCelsius == 55000);
    assert(r.state == SignalState::Valid);
    cout << "PASS: CAN wins when fresh, plausible, and not overridden by settings\n";
}

void test_forced_ntc_ignores_a_healthy_can() {
    MotorTempReading r = selectMotorTempReading(true, true, 55000, 60.0, true);
    assert(r.milliCelsius == 60000);
    assert(r.state == SignalState::Valid);
    cout << "PASS: forceNtc ignores CAN even when CAN is fresh and plausible\n";
}

void test_forced_ntc_with_dead_ntc_reports_invalid_not_the_healthy_can() {
    // The regression this whole extraction exists to prevent: forced to
    // NTC, NTC is broken -> must report Invalid, never fall back to CAN's
    // "Valid" verdict for a source that isn't even selected.
    MotorTempReading r = selectMotorTempReading(true, true, 55000, 999.0, false);
    assert(r.milliCelsius == 999000);
    assert(r.state == SignalState::Invalid);
    cout << "PASS: forced NTC that's broken reports Invalid, never borrows CAN's Valid\n";
}

void test_can_not_fresh_falls_back_to_ntc() {
    MotorTempReading r = selectMotorTempReading(false, false, 55000, 60.0, true);
    assert(r.milliCelsius == 60000);
    assert(r.state == SignalState::Valid);
    cout << "PASS: CAN not fresh -> falls back to a healthy NTC\n";
}

void test_can_implausible_falls_back_to_ntc_value_and_state_together() {
    // The other half of the original bug: CAN reports something implausible
    // (already filtered out by the caller into canFreshAndPlausible=false),
    // NTC is healthy -> both the value AND the state must come from NTC.
    MotorTempReading r = selectMotorTempReading(false, false, 200000, 60.0, true);
    assert(r.milliCelsius == 60000);
    assert(r.state == SignalState::Valid);
    cout << "PASS: CAN implausible -> value and state both come from NTC, not a mix\n";
}

void test_can_unavailable_and_ntc_broken_is_invalid() {
    MotorTempReading r = selectMotorTempReading(false, false, 0, 999.0, false);
    assert(r.state == SignalState::Invalid);
    cout << "PASS: both sources bad -> Invalid\n";
}

int main() {
    test_can_wins_when_fresh_plausible_and_not_forced_to_ntc();
    test_forced_ntc_ignores_a_healthy_can();
    test_forced_ntc_with_dead_ntc_reports_invalid_not_the_healthy_can();
    test_can_not_fresh_falls_back_to_ntc();
    test_can_implausible_falls_back_to_ntc_value_and_state_together();
    test_can_unavailable_and_ntc_broken_is_invalid();
    cout << "MotorTempSourceLogicTest: all passed" << endl;
    return 0;
}
