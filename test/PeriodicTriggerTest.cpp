#include <iostream>
#include <cassert>
#include <stdint.h>
#include <stdbool.h>
using namespace std;

#include "../src/Sound/PeriodicTrigger.h"

void test_fires_on_entry_and_every_interval() {
    PeriodicTrigger trig;
    assert(trig.update(false, 0, 1000) == false);
    assert(trig.update(true, 1000, 1000) == true);   // entry -> fires
    assert(trig.update(true, 1500, 1000) == false);  // < interval since last fire
    assert(trig.update(true, 2000, 1000) == true);   // interval elapsed -> fires
    assert(trig.update(true, 2999, 1000) == false);
    assert(trig.update(true, 3000, 1000) == true);
    cout << "PASS: fires on entry and every interval while active\n";
}

void test_resets_when_condition_clears() {
    PeriodicTrigger trig;
    assert(trig.update(true, 0, 1000) == true);
    assert(trig.update(false, 500, 1000) == false);  // cleared before the interval
    assert(trig.update(true, 600, 1000) == true);    // re-entry fires immediately
    cout << "PASS: resets when the condition clears\n";
}

void test_survives_millis_rollover() {
    PeriodicTrigger trig;
    uint32_t nearMax = 0xFFFFFFF0u; // 16ms before wraparound
    assert(trig.update(true, nearMax, 1000) == true); // entry

    uint32_t justAfterWrap = 500u;  // real elapsed since fire: 16 + 500 = 516ms
    assert(trig.update(true, justAfterWrap, 1000) == false); // 516 < 1000

    uint32_t pastInterval = 1000u;  // real elapsed: 16 + 1000 = 1016ms >= 1000
    assert(trig.update(true, pastInterval, 1000) == true);
    cout << "PASS: survives millis() rollover\n";
}

int main() {
    test_fires_on_entry_and_every_interval();
    test_resets_when_condition_clears();
    test_survives_millis_rollover();
    return 0;
}
