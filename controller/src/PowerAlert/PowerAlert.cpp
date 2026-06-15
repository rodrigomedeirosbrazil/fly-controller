#include "PowerAlert.h"
#include "../config.h"
#include "../Throttle/Throttle.h"
#include "../Power/Power.h"
#include "../Buzzer/Buzzer.h"
#include "../RemoteLink/RemoteLink.h"
#include "RemoteLinkProtocol.h"

extern Throttle throttle;
extern Power power;
extern Buzzer buzzer;
extern RemoteLink remoteLink;

PowerAlert::PowerAlert() : seq_(0), activeCauses_(0) {}

void PowerAlert::handle() {
    activeCauses_ = power.getActiveLimitCauses();

    if (logic_.update(throttle.isArmed(), activeCauses_, millis(), POWER_ALERT_BEEP_INTERVAL_MS)) {
        seq_++;
        buzzer.beepPowerAlert();
        remoteLink.requestBeep(RemoteBeep::PowerAlert);
    }
}
