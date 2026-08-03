// test/SignalStateTest.cpp
#include <cassert>
#include <iostream>
#include "../src/Telemetry/SignalState.h"
using namespace std;

void test_codes_match_table() {
    assert(signalStateCode(SignalState::Absent) == 'a');
    assert(signalStateCode(SignalState::Stale) == 's');
    assert(signalStateCode(SignalState::Invalid) == 'i');
    assert(signalStateCode(SignalState::Valid) == 'v');
    cout << "PASS: codes match the design table\n";
}

int main() {
    test_codes_match_table();
    cout << "SignalStateTest: all passed" << endl;
    return 0;
}
