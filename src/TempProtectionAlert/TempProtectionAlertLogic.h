#pragma once
#include <stdint.h>
#include <stdbool.h>

// Pure timing logic for the "temperature protection suspended" alarm -- no
// Arduino deps, host-testable.
//
// Input is the bitmask of temperature protections currently suspended
// (POWER_LIMIT_MOTOR_TEMP / POWER_LIMIT_ESC_TEMP, already debounced by
// TempProtectionMonitor). Output says which sound to play on this tick.
//
// - Lost fires the moment a protection is suspended while armed -- including
//   the tick arming succeeds with one already suspended -- and again each
//   time another protection joins the mask, restarting the reminder schedule.
// - Reminders back off: 10 s after the first alarm, then 20 s, then 30 s, and
//   every 60 s after that (alarms at 0, 10, 30, 60, 120, 180 s...). Loud at
//   first, when the pilot has to notice and decide, then only a reminder for
//   a flight that continues without the protection.
// - Restored fires once when the mask goes back to zero while armed. A partial
//   recovery (one of two protections back) plays nothing: the reminders keep
//   going, so the pilot is never told "all clear" while something is still off.
// - Disarmed is silent; the state is reset so the next arm starts fresh.
class TempProtectionAlertLogic {
public:
    struct Output {
        bool playLost;
        bool playRestored;
    };

    // Reminder schedule. Scalars rather than an array so nothing here is
    // odr-used and needs an out-of-class definition under older -std modes.
    static constexpr uint32_t kFirstReminderMs  = 10000;
    static constexpr uint32_t kSecondReminderMs = 20000;
    static constexpr uint32_t kThirdReminderMs  = 30000;
    static constexpr uint32_t kSteadyReminderMs = 60000;

    TempProtectionAlertLogic() : prevMask_(0), lastFireMs_(0), firesDone_(0) {}

    Output update(bool armed, uint8_t suspendedMask, uint32_t nowMs) {
        Output out = { false, false };

        if (!armed) {
            // prevMask_ = 0 makes an arm with a protection already suspended
            // read as "gained" on the first armed tick.
            prevMask_ = 0;
            firesDone_ = 0;
            return out;
        }

        const uint8_t gained = suspendedMask & (uint8_t)~prevMask_;
        const bool allRestored = prevMask_ != 0 && suspendedMask == 0;
        prevMask_ = suspendedMask;

        if (gained != 0) {
            firesDone_ = 0;
            fire(nowMs);
            out.playLost = true;
            return out;
        }

        if (allRestored) {
            firesDone_ = 0;
            out.playRestored = true;
            return out;
        }

        if (suspendedMask != 0 && (uint32_t)(nowMs - lastFireMs_) >= currentIntervalMs()) {
            fire(nowMs);
            out.playLost = true;
        }
        return out;
    }

private:
    uint8_t  prevMask_;
    uint32_t lastFireMs_;
    uint8_t  firesDone_;  // alarms since the schedule (re)started, saturating

    void fire(uint32_t nowMs) {
        lastFireMs_ = nowMs;
        if (firesDone_ < 255) firesDone_++;
    }

    // Wait after the Nth alarm. firesDone_ >= 1 whenever a reminder is due:
    // the schedule always starts with a "gained" fire.
    uint32_t currentIntervalMs() const {
        switch (firesDone_) {
            case 1:  return kFirstReminderMs;
            case 2:  return kSecondReminderMs;
            case 3:  return kThirdReminderMs;
            default: return kSteadyReminderMs;
        }
    }
};
