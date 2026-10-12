#pragma once

#include <stdint.h>
#include "../Settings/SettingsValidation.h"

// Remaining motor time: usable energy divided by a slow average of the current
// drawn while the motor runs. Pure: no Arduino, host-tested in
// test/RemainingTimeLogicTest.cpp. RemainingTime.{h,cpp} gathers the inputs.
//
// The average is an EMA over 10 s blocks of motor-on time (tau = 120 s). It
// starts from the configured reference current and is frozen whenever the
// motor is not running, so a glide never drags it toward zero. Without a
// current measurement it stays frozen and the energy comes from the pack
// voltage, anchored at rest and dead-reckoned while the motor runs.

enum class RemainingTimeQuality : uint8_t {
    Unavailable  = 0,
    Voltage      = 1,
    CoulombCount = 2,
};

struct RemainingTimeInput {
    uint32_t nowMs;
    bool     armed;
    bool     motorRunning;      // armed && throttle engaged (isMotorRunning())
    bool     currentAvailable;  // isCurrentAvailable()
    uint32_t currentMa;
    uint32_t ccRemainingMah;    // BatteryMonitor::getRemainingMah(): usable, above the reserve
    bool     voltageValid;      // Telemetry::isBatteryVoltageValid()
    uint32_t voltageUsableMah;  // BatteryMonitor::usableMahFromVoltage() at the current reading
};

static const uint32_t REMAINING_TIME_BLOCK_MS       = 10000;      // one EMA sample per 10 s of motor time
static const float    REMAINING_TIME_EMA_ALPHA      = 0.0799556f; // 1 - exp(-10 s / 120 s)
static const uint32_t REMAINING_TIME_PUBLISH_MS     = 10000;
static const uint32_t REMAINING_TIME_REST_SETTLE_MS = 30000;      // matches BATTERY_RECALIBRATION_STABLE_MS
static const uint32_t REMAINING_TIME_LEARN_MIN_MS   = 180000;
static const uint32_t REMAINING_TIME_MIN_DIVISOR_MA = 1000;       // never divide by less than 1 A

class RemainingTimeLogic {
public:
    // Starts the average from the reference. Called at boot and when the
    // pilot edits the setting -- never by learning, which must not throw away
    // the regime of the flight in progress.
    void seed(uint16_t referenceCurrentA) {
        referenceA_ = referenceCurrentA;
        emaMa_      = (float) referenceCurrentA * 1000.0f;
        blockMaMs_  = 0;
        blockMs_    = 0;
        published_  = false;
    }

    // Returns the learned reference current (A) on a disarm edge that
    // qualifies for learning, 0 otherwise.
    uint16_t tick(const RemainingTimeInput& in) {
        const uint32_t dt = hasLastTick_ ? (in.nowMs - lastTickMs_) : 0;
        lastTickMs_  = in.nowMs;
        hasLastTick_ = true;

        if (in.armed && !wasArmed_) {
            sessionMaMs_        = 0;
            sessionMotorMs_     = 0;
            sessionCurrentLost_ = false;
        }
        // Any loss of current while armed vetoes learning for the session,
        // including a loss during a glide, when accumulate() never looks.
        if (in.armed && !in.currentAvailable) {
            sessionCurrentLost_ = true;
        }

        accumulate(in, dt);
        const RemainingTimeQuality q = modeFor(in);
        trackVoltageAnchor(in, q, dt);
        publish(in, q);
        if (q == RemainingTimeQuality::CoulombCount) {
            lastCcMah_ = in.ccRemainingMah;
        }

        uint16_t learned = 0;
        if (wasArmed_ && !in.armed) {
            learned = learn();
        }
        wasArmed_ = in.armed;
        return learned;
    }

    uint16_t remainingMin() const { return remainingMin_; }
    RemainingTimeQuality quality() const { return quality_; }
    uint32_t averageCurrentMa() const { return (uint32_t) (emaMa_ + 0.5f); }
    uint16_t referenceCurrentA() const { return referenceA_; }

private:
    float    emaMa_      = 0.0f;
    uint16_t referenceA_ = 0;

    uint64_t blockMaMs_ = 0;
    uint32_t blockMs_   = 0;

    uint64_t sessionMaMs_        = 0;
    uint32_t sessionMotorMs_     = 0;
    bool     sessionCurrentLost_ = false;
    bool     wasArmed_           = false;

    bool     hasLastTick_ = false;
    uint32_t lastTickMs_  = 0;

    bool     hasAnchor_          = false;
    uint32_t anchorMah_          = 0;
    uint32_t motorMsSinceAnchor_ = 0;
    uint32_t restMs_             = 0;
    uint32_t lastCcMah_          = 0;

    // quality_ is also the previous tick's mode: a mode change always
    // publishes on the tick it happens, so the two never differ for long
    // enough to matter.
    RemainingTimeQuality quality_       = RemainingTimeQuality::Unavailable;
    uint16_t             remainingMin_  = 0;
    uint32_t             lastPublishMs_ = 0;
    bool                 published_     = false;

    static RemainingTimeQuality modeFor(const RemainingTimeInput& in) {
        if (in.currentAvailable) return RemainingTimeQuality::CoulombCount;
        if (in.voltageValid)     return RemainingTimeQuality::Voltage;
        return RemainingTimeQuality::Unavailable;
    }

    uint32_t divisorMa() const {
        const uint32_t ma = (uint32_t) emaMa_;
        return ma < REMAINING_TIME_MIN_DIVISOR_MA ? REMAINING_TIME_MIN_DIVISOR_MA : ma;
    }

    void accumulate(const RemainingTimeInput& in, uint32_t dt) {
        if (!in.motorRunning) {
            return;
        }
        if (!in.currentAvailable) {
            return;
        }
        const uint64_t maMs = (uint64_t) in.currentMa * dt;
        blockMaMs_      += maMs;
        blockMs_        += dt;
        sessionMaMs_    += maMs;
        sessionMotorMs_ += dt;
        if (blockMs_ >= REMAINING_TIME_BLOCK_MS) {
            const float blockAvgMa = (float) blockMaMs_ / (float) blockMs_;
            emaMa_ += REMAINING_TIME_EMA_ALPHA * (blockAvgMa - emaMa_);
            blockMaMs_ = 0;
            blockMs_   = 0;
        }
    }

    void trackVoltageAnchor(const RemainingTimeInput& in, RemainingTimeQuality q, uint32_t dt) {
        if (in.motorRunning) {
            restMs_ = 0;
            motorMsSinceAnchor_ += dt;
        } else {
            restMs_ += dt;
        }

        if (q != RemainingTimeQuality::Voltage) {
            hasAnchor_ = false;
            return;
        }
        // Current just disappeared: carry on from the coulomb count rather
        // than jumping to whatever the loaded pack voltage says. The rest
        // clock restarts too, so a voltage anchor needs a full settle after
        // the loss even if the motor had been off for longer than that.
        if (quality_ == RemainingTimeQuality::CoulombCount) {
            anchorMah_          = lastCcMah_;
            motorMsSinceAnchor_ = 0;
            restMs_             = 0;
            hasAnchor_          = true;
        }
        // Rested long enough for the open-circuit voltage to settle.
        if (restMs_ >= REMAINING_TIME_REST_SETTLE_MS) {
            anchorMah_          = in.voltageUsableMah;
            motorMsSinceAnchor_ = 0;
            hasAnchor_          = true;
        }
    }

    uint32_t energyMah(const RemainingTimeInput& in, RemainingTimeQuality q) const {
        if (q == RemainingTimeQuality::CoulombCount) {
            return in.ccRemainingMah;
        }
        if (q != RemainingTimeQuality::Voltage) {
            return 0;
        }
        if (!hasAnchor_) {
            return in.voltageUsableMah;
        }
        const uint64_t usedMah = (uint64_t) divisorMa() * motorMsSinceAnchor_ / 3600000ULL;
        const uint32_t dead = usedMah >= anchorMah_ ? 0 : (uint32_t) (anchorMah_ - usedMah);
        // The loaded voltage underestimates the charge, so it is a floor.
        return dead > in.voltageUsableMah ? dead : in.voltageUsableMah;
    }

    void publish(const RemainingTimeInput& in, RemainingTimeQuality q) {
        if (published_ && q == quality_ &&
            (in.nowMs - lastPublishMs_) < REMAINING_TIME_PUBLISH_MS) {
            return;
        }
        quality_       = q;
        lastPublishMs_ = in.nowMs;
        published_     = true;
        if (q == RemainingTimeQuality::Unavailable) {
            remainingMin_ = 0;
            return;
        }
        // mAh / mA = hours; x 60 = minutes. Integer division floors it.
        const uint64_t minutes = (uint64_t) energyMah(in, q) * 60ULL / divisorMa();
        remainingMin_ = minutes > 65535ULL ? 65535 : (uint16_t) minutes;
    }

    uint16_t learn() {
        if (sessionMotorMs_ < REMAINING_TIME_LEARN_MIN_MS || sessionCurrentLost_) {
            return 0;
        }
        const float flightAvgA = (float) sessionMaMs_ / (float) sessionMotorMs_ / 1000.0f;
        // A real flight never averages under the reference floor; an average
        // that low is a sensor reporting zero, and must not drag the seed down.
        if (flightAvgA < (float) REFERENCE_CURRENT_MIN_A) {
            return 0;
        }
        const float blended    = ((float) referenceA_ + flightAvgA) / 2.0f;
        referenceA_ = clampReferenceCurrent((uint32_t) (blended + 0.5f));
        return referenceA_;
    }
};
