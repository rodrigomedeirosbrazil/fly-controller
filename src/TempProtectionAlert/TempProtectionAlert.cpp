#include "TempProtectionAlert.h"
#include <Arduino.h>
#include "../Throttle/Throttle.h"
#include "../Power/Power.h"
#include "../Sound/Sound.h"

extern Throttle throttle;
extern Power power;
extern Sound sound;

void TempProtectionAlert::handle() {
    TempProtectionAlertLogic::Output out = logic_.update(
        throttle.isArmed(), power.getSuspendedTempProtections(), millis());

    if (out.playLost) {
        sound.play(SoundEvent::TempProtectionLostHigh);
        sound.play(SoundEvent::TempProtectionLostLow);
    }
    if (out.playRestored) {
        sound.play(SoundEvent::TempProtectionRestoredLow);
        sound.play(SoundEvent::TempProtectionRestoredHigh);
    }
}
