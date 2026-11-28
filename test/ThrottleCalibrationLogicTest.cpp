#include <iostream>
#include <cassert>
#include <cstdlib>
using namespace std;

#include "../src/Throttle/ThrottleCalibrationLogic.h"

// Feeds a constant value every 10 ms from fromMs to toMs inclusive (wrapping
// uint32_t) and returns the last non-None event.
static CalibrationEvent feed(ThrottleCalibrationLogic &c, int value, uint32_t fromMs, uint32_t toMs) {
    CalibrationEvent last = CalibrationEvent::None;
    const uint32_t steps = (toMs - fromMs) / 10;
    for (uint32_t i = 0; i <= steps; ++i) {
        CalibrationEvent e = c.update(value, fromMs + i * 10);
        if (e != CalibrationEvent::None) last = e;
    }
    return last;
}

static void normalProcedure(uint32_t t0) {
    ThrottleCalibrationLogic c;
    assert(feed(c, 900, t0, t0 + 500) == CalibrationEvent::None);       // at rest after power-on
    assert(feed(c, 2400, t0 + 510, t0 + 3510) == CalibrationEvent::MaxCaptured && c.pinMax() == 2400);
    assert(c.step() == 1);
    assert(feed(c, 900, t0 + 3520, t0 + 6520) == CalibrationEvent::Complete);
    assert(c.isCalibrated() && c.pinMin() == 900 && c.pinMax() == 2400);
    // frozen after completion
    assert(feed(c, 1500, t0 + 6530, t0 + 12000) == CalibrationEvent::None);
    assert(c.pinMin() == 900 && c.pinMax() == 2400);
    c.reset();
    assert(!c.isCalibrated() && c.step() == 0);
}

int main() {
    normalProcedure(0);
    cout << "PASS: normalProcedure\n";

    {   // untouched throttle: no event at all (no "release" cue every few seconds)
        ThrottleCalibrationLogic c;
        for (uint32_t t = 0; t <= 30000; t += 10) {
            assert(c.update(900, t) == CalibrationEvent::None);
        }
        assert(c.step() == 0 && !c.isCalibrated());
    }
    cout << "PASS: untouchedThrottleIsSilent\n";

    {   // held open at power-on: never accepted until released and squeezed again
        ThrottleCalibrationLogic c;
        assert(feed(c, 2400, 0, 10000) == CalibrationEvent::None);
        assert(c.step() == 0);
        assert(feed(c, 900, 10010, 11000) == CalibrationEvent::None);
        assert(feed(c, 2400, 11010, 14010) == CalibrationEvent::MaxCaptured && c.pinMax() == 2400);
        assert(feed(c, 900, 14020, 17020) == CalibrationEvent::Complete);
    }
    cout << "PASS: throttleOpenAtPowerOnNeedsRelease\n";

    {   // a squeeze smaller than MIN_SPAN above rest is not a max
        ThrottleCalibrationLogic c;
        assert(feed(c, 900, 0, 500) == CalibrationEvent::None);
        assert(feed(c, 900 + ThrottleCalibrationLogic::MIN_SPAN - 1, 510, 10000) == CalibrationEvent::None);
        assert(c.step() == 0);
    }
    cout << "PASS: smallSqueezeIgnored\n";

    {   // resqueeze during the min step
        ThrottleCalibrationLogic c;
        assert(feed(c, 900, 0, 500) == CalibrationEvent::None);
        assert(feed(c, 1700, 510, 3510) == CalibrationEvent::MaxCaptured && c.pinMax() == 1700);
        assert(c.update(2400, 3520) == CalibrationEvent::None);
        assert(c.step() == 0);
        assert(feed(c, 2400, 3530, 6530) == CalibrationEvent::MaxCaptured && c.pinMax() == 2400);
        assert(feed(c, 900, 6540, 9540) == CalibrationEvent::Complete);
        assert(c.pinMin() == 900 && c.pinMax() == 2400);
    }
    cout << "PASS: resqueezeDuringMinStep\n";

    {   // partial release after the cue
        ThrottleCalibrationLogic c;
        assert(feed(c, 900, 0, 500) == CalibrationEvent::None);
        assert(feed(c, 1600, 510, 3510) == CalibrationEvent::MaxCaptured);
        assert(feed(c, 1200, 3520, 6520) == CalibrationEvent::RestartWithBeep);
        assert(c.step() == 0 && !c.isCalibrated());
    }
    cout << "PASS: partialReleaseBeeps\n";

    {   // still holding full after the cue: silent restart, then the cue again
        ThrottleCalibrationLogic c;
        assert(feed(c, 900, 0, 500) == CalibrationEvent::None);
        assert(feed(c, 2400, 510, 3510) == CalibrationEvent::MaxCaptured);
        assert(feed(c, 2400, 3520, 6520) == CalibrationEvent::Restart);
        assert(feed(c, 2400, 6530, 9530) == CalibrationEvent::MaxCaptured);
    }
    cout << "PASS: holdingTooLongCuesAgain\n";

    {   // unstable hold never completes
        ThrottleCalibrationLogic c;
        feed(c, 900, 0, 500);
        for (uint32_t t = 510; t <= 10000; t += 10) {
            int v = ((t / 10) % 2) ? 2500 : 2400;
            assert(c.update(v, t) != CalibrationEvent::MaxCaptured);
        }
        assert(c.step() == 0);
    }
    cout << "PASS: unstableHoldNeverCompletes\n";

    {   // hold value is the mean, not the last sample
        ThrottleCalibrationLogic c;
        feed(c, 900, 0, 500);
        CalibrationEvent e = CalibrationEvent::None;
        for (uint32_t i = 0; i <= 300; ++i) {
            int v = 2380 + (int)((i % 21) * 2);   // 2380..2420, mean 2400
            CalibrationEvent x = c.update(v, 510 + i * 10);
            if (x != CalibrationEvent::None) e = x;
        }
        assert(e == CalibrationEvent::MaxCaptured);
        assert(abs(c.pinMax() - 2400) <= 2);
    }
    cout << "PASS: holdMeanNotLastSample\n";

    normalProcedure(0xFFFFF000u);
    cout << "PASS: rollover\n";
    return 0;
}
