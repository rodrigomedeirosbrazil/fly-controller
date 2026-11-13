#pragma once
#include <stdint.h>
#include "BmsAlertLogic.h"
#include "../TempProtectionAlert/TempProtectionAlertLogic.h"

// Raises three BMS alerts (low cell, high delta, high temperature) from the
// BMS readings and the persisted thresholds. Informational only: the alerts
// never reach Power, never limit the motor and never disarm. The bitmask is
// shown in TELEMETRY at all times; sound plays only while armed, on the
// TempProtectionAlertLogic cadence. Decision logic lives in BmsAlertLogic.h.
class BmsAlert {
public:
    void handle();
    uint8_t getActiveAlerts() const { return logic_.getMask(); }

private:
    BmsAlertLogic logic_;
    TempProtectionAlertLogic soundLogic_;
};
