// test/TempProtectionMonitorTest.cpp
#include <cassert>
#include <iostream>
#include "../src/Power/TempProtectionMonitor.h"
using namespace std;

static const uint32_t DEBOUNCE = 2000;

void test_starts_not_suspended() {
    TempProtectionMonitor m;
    assert(!m.isSuspended());
    cout << "PASS: starts not suspended\n";
}

void test_suspends_only_after_debounce() {
    TempProtectionMonitor m;
    m.update(false, 0, DEBOUNCE);
    assert(!m.isSuspended());
    m.update(false, DEBOUNCE - 1, DEBOUNCE);
    assert(!m.isSuspended());
    m.update(false, DEBOUNCE, DEBOUNCE);
    assert(m.isSuspended());
    cout << "PASS: suspends after the debounce window of continuous loss\n";
}

void test_brief_dropout_never_suspends() {
    TempProtectionMonitor m;
    m.update(false, 0, DEBOUNCE);
    m.update(false, 1900, DEBOUNCE);
    m.update(true, 1950, DEBOUNCE);   // one good sample resets the window
    m.update(false, 2000, DEBOUNCE);
    m.update(false, 3999, DEBOUNCE);  // 1999 ms into the new episode
    assert(!m.isSuspended());
    m.update(false, 4000, DEBOUNCE);
    assert(m.isSuspended());
    cout << "PASS: a dropout shorter than the window is ignored\n";
}

void test_restores_only_after_debounce() {
    TempProtectionMonitor m;
    m.update(false, 0, DEBOUNCE);
    m.update(false, DEBOUNCE, DEBOUNCE);
    assert(m.isSuspended());

    m.update(true, 5000, DEBOUNCE);
    m.update(true, 5000 + DEBOUNCE - 1, DEBOUNCE);
    assert(m.isSuspended());
    m.update(true, 5000 + DEBOUNCE, DEBOUNCE);
    assert(!m.isSuspended());
    cout << "PASS: restores after the debounce window of continuous validity\n";
}

void test_flapping_signal_holds_suspended() {
    // A bus that comes back for less than the window each time must not
    // alternate lost/restored.
    TempProtectionMonitor m;
    m.update(false, 0, DEBOUNCE);
    m.update(false, DEBOUNCE, DEBOUNCE);
    assert(m.isSuspended());
    for (uint32_t t = 3000; t < 30000; t += 1000) {
        m.update(true, t, DEBOUNCE);
        m.update(false, t + 500, DEBOUNCE);
        assert(m.isSuspended());
    }
    cout << "PASS: a flapping signal stays suspended\n";
}

void test_millis_rollover() {
    TempProtectionMonitor m;
    const uint32_t nearMax = 0xFFFFFFFFu - 500;
    m.update(false, nearMax, DEBOUNCE);
    m.update(false, 1000, DEBOUNCE);   // 1501 ms elapsed across the wrap
    assert(!m.isSuspended());
    m.update(false, 1499, DEBOUNCE);   // 2000 ms elapsed
    assert(m.isSuspended());
    cout << "PASS: debounce is correct across millis() rollover\n";
}

int main() {
    test_starts_not_suspended();
    test_suspends_only_after_debounce();
    test_brief_dropout_never_suspends();
    test_restores_only_after_debounce();
    test_flapping_signal_holds_suspended();
    test_millis_rollover();
    cout << "TempProtectionMonitorTest: all passed" << endl;
    return 0;
}
