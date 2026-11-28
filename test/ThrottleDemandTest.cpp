#include <iostream>
#include <cassert>
using namespace std;

#include "../src/Throttle/ThrottleDemand.h"

// pinMin 1000, pinMax 2000 -> engage point 1020, top point 1970
int main() {
    assert(throttleDemandPpm(1500, 1000, 2000, false) == 0);          // gate released
    assert(throttleDemandPpm(1015, 1000, 2000, true)  == 0);          // hysteresis band, engaged
    assert(throttleDemandPpm(1020, 1000, 2000, true)  == 0);          // engage point
    assert(throttleDemandPpm(1495, 1000, 2000, true)  == 500000);     // midpoint
    assert(throttleDemandPpm(1970, 1000, 2000, true)  == 1000000);    // top point
    assert(throttleDemandPpm(2100, 1000, 2000, true)  == 1000000);    // above max
    assert(throttleDemandPpm(1500, 1000, 1000, true)  == 0);          // zero range
    cout << "PASS: demand mapping anchors\n";

    // continuity: each count above the engage point adds < 0.2 %
    for (int f = 1020; f < 1970; ++f) {
        assert(throttleDemandPpm(f + 1, 1000, 2000, true) - throttleDemandPpm(f, 1000, 2000, true) < 2000);
    }
    cout << "PASS: demand is continuous\n";

    assert(demandPercent(0) == 0 && demandPercent(4999) == 0 && demandPercent(5000) == 1);
    assert(demandPercent(1000000) == 100);
    cout << "PASS: demandPercent rounding\n";
    return 0;
}
