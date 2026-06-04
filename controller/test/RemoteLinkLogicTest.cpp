#include <cassert>
#include <cstdint>
#include <iostream>
#include "../src/RemoteLink/RemoteLinkLogic.h"
using namespace std;

void test_no_failsafe_when_wired() {
    // Wired mode never triggers failsafe, even with a huge gap.
    assert(computeFailsafe(false, 0, 100000) == FailsafeAction::None);
}

void test_no_failsafe_within_window() {
    assert(computeFailsafe(true, 1000, 1000 + FAILSAFE_RAMP_MS - 1) == FailsafeAction::None);
}

void test_ramp_after_ramp_threshold() {
    assert(computeFailsafe(true, 1000, 1000 + FAILSAFE_RAMP_MS + 1) == FailsafeAction::RampToZero);
}

void test_disarm_after_disarm_threshold() {
    assert(computeFailsafe(true, 1000, 1000 + FAILSAFE_DISARM_MS + 1) == FailsafeAction::Disarm);
}

int main() {
    test_no_failsafe_when_wired();
    test_no_failsafe_within_window();
    test_ramp_after_ramp_threshold();
    test_disarm_after_disarm_threshold();
    cout << "RemoteLinkLogicTest: all passed" << endl;
    return 0;
}
