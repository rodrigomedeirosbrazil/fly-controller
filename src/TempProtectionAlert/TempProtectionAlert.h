#pragma once
#include <stdint.h>
#include "TempProtectionAlertLogic.h"

// Alarm for a temperature protection (motor or ESC) that stopped working in
// flight. Never cuts the motor: Power simply stops derating on that signal,
// and this tells the pilot. Timing lives in TempProtectionAlertLogic.
class TempProtectionAlert {
public:
    void handle();

private:
    TempProtectionAlertLogic logic_;
};
