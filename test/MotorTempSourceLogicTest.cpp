#include <cassert>
#include <iostream>
#include "../src/Tmotor/MotorTempSourceLogic.h"
using namespace std;

void test_can_wins_when_fresh_plausible_and_not_forced_to_ntc() {
    MotorTempReading r = selectMotorTempReading(false, true, 55000, 60.0, true);
    assert(r.milliCelsius == 55000);
    assert(r.state == SignalState::Valid);
    assert(r.origin == MotorTempOrigin::Can);
    cout << "PASS: CAN wins when fresh, plausible, and not overridden by settings\n";
}

void test_forced_ntc_ignores_a_healthy_can() {
    MotorTempReading r = selectMotorTempReading(true, true, 55000, 60.0, true);
    assert(r.milliCelsius == 60000);
    assert(r.state == SignalState::Valid);
    assert(r.origin == MotorTempOrigin::Ntc);
    cout << "PASS: forceNtc ignores CAN even when CAN is fresh and plausible\n";
}

void test_forced_ntc_with_dead_ntc_reports_invalid_not_the_healthy_can() {
    // The regression this whole extraction exists to prevent: forced to
    // NTC, NTC is broken -> must report Invalid, never fall back to CAN's
    // "Valid" verdict for a source that isn't even selected.
    MotorTempReading r = selectMotorTempReading(true, true, 55000, 999.0, false);
    assert(r.milliCelsius == 999000);
    assert(r.state == SignalState::Invalid);
    assert(r.origin == MotorTempOrigin::Ntc);
    cout << "PASS: forced NTC that's broken reports Invalid, never borrows CAN's Valid\n";
}

void test_can_not_fresh_falls_back_to_ntc() {
    MotorTempReading r = selectMotorTempReading(false, false, 55000, 60.0, true);
    assert(r.milliCelsius == 60000);
    assert(r.state == SignalState::Valid);
    assert(r.origin == MotorTempOrigin::Ntc);
    cout << "PASS: CAN not fresh -> falls back to a healthy NTC\n";
}

void test_can_implausible_falls_back_to_ntc_value_and_state_together() {
    // The other half of the original bug: CAN reports something implausible
    // (already filtered out by the caller into canFreshAndPlausible=false),
    // NTC is healthy -> both the value AND the state must come from NTC.
    MotorTempReading r = selectMotorTempReading(false, false, 200000, 60.0, true);
    assert(r.milliCelsius == 60000);
    assert(r.state == SignalState::Valid);
    assert(r.origin == MotorTempOrigin::Ntc);
    cout << "PASS: CAN implausible -> value and state both come from NTC, not a mix\n";
}

void test_can_unavailable_and_ntc_broken_is_invalid() {
    MotorTempReading r = selectMotorTempReading(false, false, 0, 999.0, false);
    assert(r.state == SignalState::Invalid);
    assert(r.origin == MotorTempOrigin::Ntc);
    cout << "PASS: both sources bad -> Invalid\n";
}

void test_origin_is_ntc_even_when_the_ntc_is_invalid() {
    // The origin describes WHICH sensor was consulted, not whether it
    // answered sensibly — a dead NTC still reports Ntc alongside Invalid.
    MotorTempReading r = selectMotorTempReading(true, true, 55000, 999.0, false);
    assert(r.milliCelsius == 999000);
    assert(r.state == SignalState::Invalid);
    assert(r.origin == MotorTempOrigin::Ntc);
    cout << "PASS: origin is Ntc even when the NTC itself is invalid\n";
}

void test_origin_matches_the_value_in_every_branch() {
    // Table over the four (forceNtc x canFreshAndPlausible) combinations.
    // Distinct CAN/NTC values so origin and value can't be confused:
    // 55000 is CAN, 60500 is NTC.
    const int32_t CAN = 55000;
    const int32_t NTC = (int32_t)(60.5 * 1000.0);
    const bool force[] = { false, false, true, true };
    const bool fresh[] = { false, true, false, true };
    for (int i = 0; i < 4; i++) {
        MotorTempReading r = selectMotorTempReading(force[i], fresh[i], CAN, 60.5, true);
        assert((r.origin == MotorTempOrigin::Can) == (r.milliCelsius == CAN));
        assert((r.origin == MotorTempOrigin::Ntc) == (r.milliCelsius == NTC));
    }
    cout << "PASS: origin matches the value in every branch\n";
}

int main() {
    test_can_wins_when_fresh_plausible_and_not_forced_to_ntc();
    test_forced_ntc_ignores_a_healthy_can();
    test_forced_ntc_with_dead_ntc_reports_invalid_not_the_healthy_can();
    test_can_not_fresh_falls_back_to_ntc();
    test_can_implausible_falls_back_to_ntc_value_and_state_together();
    test_can_unavailable_and_ntc_broken_is_invalid();
    test_origin_is_ntc_even_when_the_ntc_is_invalid();
    test_origin_matches_the_value_in_every_branch();
    cout << "MotorTempSourceLogicTest: all passed" << endl;
    return 0;
}
