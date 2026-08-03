#include <cassert>
#include <iostream>
#include "../src/RemoteLink/RemoteLink.h"
using namespace std;

static const uint32_t kWindow = 100;

void test_fresh_within_window() {
    assert(isGapFresh(1000, 1050, kWindow));   // 50ms gap, within 100ms window
    cout << "PASS: within-window gap is fresh\n";
}

void test_stale_beyond_window() {
    assert(!isGapFresh(1000, 1101, kWindow));  // 101ms gap, beyond 100ms window
    cout << "PASS: beyond-window gap is stale\n";
}

void test_exactly_at_window_boundary() {
    assert(!isGapFresh(1000, 1100, kWindow));  // exactly 100ms — not < windowMs
    cout << "PASS: exactly-at-boundary gap is stale (strict less-than)\n";
}

void test_underflow_guard() {
    // Simulates the ISR race: lastRxMs is newer than nowMs (the recv callback
    // fired between the caller's millis() snapshot and this read). The
    // unsigned subtraction underflows to a huge value; the guard must treat
    // this as fresh, not as a multi-year-old gap.
    assert(isGapFresh(1005, 1000, kWindow));
    assert(isGapFresh(50000, 49990, kWindow));
    cout << "PASS: ISR-race underflow is treated as fresh, not stale\n";
}

int main() {
    test_fresh_within_window();
    test_stale_beyond_window();
    test_exactly_at_window_boundary();
    test_underflow_guard();
    cout << "RemoteLinkFreshnessTest: all passed" << endl;
    return 0;
}
