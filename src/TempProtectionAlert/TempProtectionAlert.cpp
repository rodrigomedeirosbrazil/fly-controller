#include "TempProtectionAlert.h"
#include <Arduino.h>
#include "../Throttle/Throttle.h"
#include "../Power/Power.h"
#include "../Sound/Sound.h"
#include "../RemoteLink/RemoteLink.h"
#include "../RemoteLink/RemoteLinkProtocol.h"

extern Throttle throttle;
extern Power power;
extern Sound sound;
extern RemoteLink remoteLink;

void TempProtectionAlert::handle() {
    TempProtectionAlertLogic::Output out = logic_.update(
        throttle.isArmed(), power.getSuspendedTempProtections(), millis());

    if (out.playLost) {
        sound.play(SoundEvent::TempProtectionLostHigh);
        sound.play(SoundEvent::TempProtectionLostLow);
        // The remote has no pattern of its own for this yet; its generic
        // alert is enough to make the pilot look at the app.
        remoteLink.requestBeep(RemoteBeep::Alert);
    }
    if (out.playRestored) {
        sound.play(SoundEvent::TempProtectionRestoredLow);
        sound.play(SoundEvent::TempProtectionRestoredHigh);
    }
}
