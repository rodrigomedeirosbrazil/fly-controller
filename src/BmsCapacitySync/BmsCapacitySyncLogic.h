#pragma once

#include <stdint.h>
#include "../Settings/SettingsValidation.h"

// Pure decision for adopting the BMS-reported nominal capacity as the capacity
// the coulomb count uses. Runtime only: the persisted setting is never touched,
// since the BMS reports the value again on every connection. Header-only and
// free of Arduino deps, host-tested in test/BmsCapacitySyncLogicTest.cpp like
// the other *Logic headers.
//
// Evaluated every loop tick. A value is applied only when it is:
//   - nonzero (0 = no BMS, Daly, or a JK whose capacity is not set),
//   - unchanged for BMS_CAPACITY_STABLE_MS -- a misdecoded JK frame (wrong
//     aggregate base) lasts one frame (~1 s) and is replaced by the next,
//   - inside the CFG_SET range (validateCapacity), else reported once,
//   - different from the BMS capacity already adopted (0 before the first),
//   - and the controller is disarmed; while armed it stays pending and is
//     applied on the first disarmed tick, with no fresh window.
//
// There is deliberately no opt-out: while a BMS reports a capacity it wins over
// the configured one, which stays saved for when no BMS reports one.

#define BMS_CAPACITY_STABLE_MS 3000

struct BmsCapacitySyncInput {
    uint32_t nowMs;
    uint32_t bmsCapacityMah;    // 0 = BMS absent / not reporting
    uint32_t adoptedMah;        // BatteryMonitor::getBmsCapacity(), 0 = none yet
    bool     armed;
};

enum class BmsCapacitySyncAction : uint8_t { None, Apply, OutOfRange };

struct BmsCapacitySyncDecision {
    BmsCapacitySyncAction action;
    uint32_t mah;               // value to apply, or the one rejected
};

class BmsCapacitySyncLogic {
public:
    BmsCapacitySyncDecision update(const BmsCapacitySyncInput& in) {
        const BmsCapacitySyncDecision none = { BmsCapacitySyncAction::None, 0 };

        if (in.bmsCapacityMah == 0) {
            candidateMah_ = 0;
            return none;
        }
        if (in.bmsCapacityMah != candidateMah_) {
            candidateMah_     = in.bmsCapacityMah;
            candidateSinceMs_ = in.nowMs;
            return none;
        }
        if (in.nowMs - candidateSinceMs_ < BMS_CAPACITY_STABLE_MS) {
            return none;
        }
        if (validateCapacity(candidateMah_) != SettingsError::None) {
            if (candidateMah_ == rejectedMah_) {
                return none;
            }
            rejectedMah_ = candidateMah_;
            return { BmsCapacitySyncAction::OutOfRange, candidateMah_ };
        }
        if (candidateMah_ == in.adoptedMah || in.armed) {
            return none;
        }
        return { BmsCapacitySyncAction::Apply, candidateMah_ };
    }

private:
    uint32_t candidateMah_     = 0;
    uint32_t candidateSinceMs_ = 0;
    uint32_t rejectedMah_      = 0;   // last value reported as OutOfRange
};
