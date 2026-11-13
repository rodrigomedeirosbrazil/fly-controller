#pragma once

// Pure decision logic for the three BMS alerts (low cell, high delta, high
// temperature). No Arduino includes: host-tested in test/BmsAlertLogicTest.cpp.
//
// The alerts are informational only. They never feed Power, never limit and
// never disarm.
//
// Why the delta gate on low cell: with a small delta all cells reach the
// cutoff together, and the existing pack-voltage limiter already covers that.
// The low-cell alert targets one weak cell hitting the cutoff while the pack
// average is still above it, so it needs the cell spread to exceed a gate.
//
// Each alert is a raw condition with hysteresis, followed by a debounced
// output that changes only after the raw state has differed from it
// continuously for BMS_ALERT_DEBOUNCE_MS (throttle bursts sag the weakest
// cell). An unavailable data source clears its alerts at once, so a dropped
// BMS link never leaves a stale alert on.

#include <stdint.h>
#include <stdbool.h>

namespace BmsAlertBit {
    // Wire values, pinned in ControlProtocolTest.
    static const uint8_t LowCell   = 0x01;
    static const uint8_t HighDelta = 0x02;
    static const uint8_t HighTemp  = 0x04;
}

static const uint32_t BMS_ALERT_DEBOUNCE_MS             = 3000;
static const uint16_t BMS_ALERT_LOW_CELL_EXIT_MARGIN_MV = 50;
static const uint16_t BMS_ALERT_DELTA_EXIT_MARGIN_MV    = 10;
static const int16_t  BMS_ALERT_TEMP_EXIT_MARGIN_C      = 3;

// Per-cell cutoff from the pack minimum voltage. 0 when cellCount == 0
// (low-cell alert disabled).
inline uint16_t perCellCutoffMv(uint16_t packMinMv, uint8_t cellCount) {
    if (cellCount == 0) return 0;
    return (uint16_t)(packMinMv / cellCount);
}

struct BmsAlertReading {
    bool     cellDataAvailable;
    uint16_t cellMinMv;
    uint16_t cellDeltaMv;
    bool     tempAvailable;
    int16_t  tempMaxC;
};

struct BmsAlertThresholds {
    uint16_t cellCutoffMv;        // 0 = low-cell alert disabled
    uint16_t lowCellDeltaGateMv;
    uint16_t maxDeltaMv;
    int16_t  tempLimitC;
};

class BmsAlertLogic {
public:
    uint8_t update(const BmsAlertReading& r, const BmsAlertThresholds& t, uint32_t nowMs) {
        // Low cell
        bool lowRaw = false;
        if (r.cellDataAvailable && t.cellCutoffMv > 0) {
            if (lowCell_.raw) {
                const bool exit = (int32_t)r.cellMinMv >= (int32_t)t.cellCutoffMv + BMS_ALERT_LOW_CELL_EXIT_MARGIN_MV
                               || (int32_t)r.cellDeltaMv < (int32_t)t.lowCellDeltaGateMv - BMS_ALERT_DELTA_EXIT_MARGIN_MV;
                lowRaw = !exit;
            } else {
                lowRaw = r.cellMinMv <= t.cellCutoffMv && r.cellDeltaMv > t.lowCellDeltaGateMv;
            }
        }
        step(lowCell_, r.cellDataAvailable, lowRaw, nowMs);

        // High delta
        bool deltaRaw = false;
        if (r.cellDataAvailable) {
            if (highDelta_.raw) {
                deltaRaw = (int32_t)r.cellDeltaMv >= (int32_t)t.maxDeltaMv - BMS_ALERT_DELTA_EXIT_MARGIN_MV;
            } else {
                deltaRaw = r.cellDeltaMv > t.maxDeltaMv;
            }
        }
        step(highDelta_, r.cellDataAvailable, deltaRaw, nowMs);

        // High temperature
        bool tempRaw = false;
        if (r.tempAvailable) {
            if (highTemp_.raw) {
                tempRaw = (int32_t)r.tempMaxC >= (int32_t)t.tempLimitC - BMS_ALERT_TEMP_EXIT_MARGIN_C;
            } else {
                tempRaw = r.tempMaxC >= t.tempLimitC;
            }
        }
        step(highTemp_, r.tempAvailable, tempRaw, nowMs);

        return getMask();
    }

    uint8_t getMask() const {
        uint8_t m = 0;
        if (lowCell_.out)   m |= BmsAlertBit::LowCell;
        if (highDelta_.out) m |= BmsAlertBit::HighDelta;
        if (highTemp_.out)  m |= BmsAlertBit::HighTemp;
        return m;
    }

private:
    struct Channel {
        bool     raw   = false;
        bool     out   = false;
        bool     timing = false;
        uint32_t since = 0;
    };

    static void step(Channel& c, bool available, bool raw, uint32_t nowMs) {
        if (!available) {
            c.raw = false;
            c.out = false;
            c.timing = false;
            return;
        }
        c.raw = raw;
        if (raw == c.out) {
            c.timing = false;
            return;
        }
        if (!c.timing) {
            c.timing = true;
            c.since  = nowMs;
        }
        if ((uint32_t)(nowMs - c.since) >= BMS_ALERT_DEBOUNCE_MS) {
            c.out    = raw;
            c.timing = false;
        }
    }

    Channel lowCell_;
    Channel highDelta_;
    Channel highTemp_;
};
