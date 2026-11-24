#include "PowerAlert.h"
#include "../config.h"
#include "../Throttle/Throttle.h"
#include "../Power/Power.h"
#include "../Sound/Sound.h"

extern Throttle throttle;
extern Power power;
extern Sound sound;

PowerAlert::PowerAlert() : activeCauses_(0) {}

void PowerAlert::handle() {
    activeCauses_ = power.getActiveLimitCauses();

    if (logic_.update(throttle.isArmed(), activeCauses_, power.isThrottleClipped(), millis(), POWER_ALERT_BEEP_INTERVAL_MS)) {
        sound.play(SoundEvent::PowerAlert);
    }
}
