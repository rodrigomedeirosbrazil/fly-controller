#include <iostream>
#include <cassert>
using namespace std;

// Pull in the pure logic header (no Arduino deps)
#include "../src/Throttle/ThrottleEngagementLogic.h"

// pinMin=1000, pinMax=2000 -> range=1000, engage at 1000+2%=1020, release at 1000+1%=1010
int main() {
    // Below engage threshold at rest — stays disengaged
    {
        ThrottleEngagementLogic logic;
        assert(!logic.update(1000, 1000, 2000));
        assert(!logic.update(1010, 1000, 2000)); // exactly at release threshold, not above engage
        cout << "PASS: stays disengaged below engage threshold\n";
    }

    // Crossing above engage threshold — engages
    {
        ThrottleEngagementLogic logic;
        assert(logic.update(1021, 1000, 2000));
        assert(logic.isEngaged());
        cout << "PASS: engages just above threshold\n";
    }

    // Hysteresis: once engaged, stays engaged between release and engage thresholds
    {
        ThrottleEngagementLogic logic;
        assert(logic.update(1021, 1000, 2000));  // engage
        assert(logic.update(1015, 1000, 2000));  // between 1010 and 1020 — still engaged
        cout << "PASS: hysteresis holds engagement mid-band\n";
    }

    // Dropping below release threshold — disengages
    {
        ThrottleEngagementLogic logic;
        logic.update(1021, 1000, 2000);          // engage
        assert(!logic.update(1005, 1000, 2000)); // below release threshold
        cout << "PASS: releases below release threshold\n";
    }

    // Uncalibrated (zero range) — never engages regardless of reading
    {
        ThrottleEngagementLogic logic;
        assert(!logic.update(5000, 1000, 1000));
        cout << "PASS: never engages with zero calibration range\n";
    }

    // Full stick immediately — engages on the very first sample
    {
        ThrottleEngagementLogic logic;
        assert(logic.update(2000, 1000, 2000));
        cout << "PASS: engages immediately on full-stick input\n";
    }

    // Noise below engage threshold never triggers engagement
    {
        ThrottleEngagementLogic logic;
        assert(!logic.update(1000, 1000, 2000));
        assert(!logic.update(1015, 1000, 2000));
        assert(!logic.update(1000, 1000, 2000));
        assert(!logic.update(1015, 1000, 2000));
        cout << "PASS: noise below engage threshold never engages\n";
    }

    // reset() forces disengagement (used when calibration restarts)
    {
        ThrottleEngagementLogic logic;
        logic.update(2000, 1000, 2000); // engage
        logic.reset();
        assert(!logic.isEngaged());
        cout << "PASS: reset() forces disengagement\n";
    }

    cout << "All ThrottleEngagementLogic tests passed!\n";
    return 0;
}
