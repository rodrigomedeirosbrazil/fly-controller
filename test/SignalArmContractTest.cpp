// test/SignalArmContractTest.cpp
#include <cassert>
#include <iostream>
#include "../src/Power/SignalArmContract.h"
using namespace std;

void test_valid_at_arm_limits_while_still_valid() {
    SignalArmContract c;
    c.onArmed(true);
    assert(c.shouldLimit(true));
    cout << "PASS: valid at arm, still valid -> limiting applies\n";
}

void test_valid_at_arm_then_invalid_disarms() {
    SignalArmContract c;
    c.onArmed(true);
    assert(c.shouldDisarmOnLoss(false));
    cout << "PASS: valid at arm, now invalid -> disarm\n";
}

void test_valid_at_arm_then_invalid_stops_limiting_too() {
    SignalArmContract c;
    c.onArmed(true);
    assert(!c.shouldLimit(false));
    cout << "PASS: valid at arm, now invalid -> no longer limits (disarm handles the motor)\n";
}

void test_invalid_at_arm_never_limits_even_if_it_recovers() {
    SignalArmContract c;
    c.onArmed(false);
    assert(!c.shouldLimit(true));
    cout << "PASS: invalid at arm -> disabled all session, even after recovering\n";
}

void test_invalid_at_arm_never_disarms() {
    SignalArmContract c;
    c.onArmed(false);
    assert(!c.shouldDisarmOnLoss(false));
    assert(!c.shouldDisarmOnLoss(true));
    cout << "PASS: invalid at arm -> never triggers a disarm from this signal\n";
}

void test_still_valid_never_triggers_disarm() {
    SignalArmContract c;
    c.onArmed(true);
    assert(!c.shouldDisarmOnLoss(true));
    cout << "PASS: still valid -> no disarm\n";
}

void test_rearming_takes_a_fresh_snapshot() {
    SignalArmContract c;
    c.onArmed(true);
    assert(c.shouldDisarmOnLoss(false)); // goes invalid, disarms
    c.onArmed(false); // pilot re-arms with the sensor still bad
    assert(!c.shouldLimit(true));        // disabled all session now
    assert(!c.shouldDisarmOnLoss(false));
    cout << "PASS: a fresh onArmed() call replaces the previous session's contract\n";
}

int main() {
    test_valid_at_arm_limits_while_still_valid();
    test_valid_at_arm_then_invalid_disarms();
    test_valid_at_arm_then_invalid_stops_limiting_too();
    test_invalid_at_arm_never_limits_even_if_it_recovers();
    test_invalid_at_arm_never_disarms();
    test_still_valid_never_triggers_disarm();
    test_rearming_takes_a_fresh_snapshot();
    cout << "SignalArmContractTest: all passed" << endl;
    return 0;
}
