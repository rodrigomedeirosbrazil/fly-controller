#pragma once

#include <Arduino.h>
#include "RemainingTimeLogic.h"

// Arduino wrapper over RemainingTimeLogic: gathers the inputs each loop tick
// and persists a learned reference current. Transports (BleControl, Xctod,
// TelemetryLogger) only read the getters.
class RemainingTime {
public:
    void init();
    void handle(bool isArmed, bool motorRunning);
    // The pilot changed the reference (CFG_SET): restart the average from it.
    void setReferenceCurrentA(uint16_t amps);

    uint16_t getRemainingMin() const { return logic_.remainingMin(); }
    RemainingTimeQuality getQuality() const { return logic_.quality(); }
    uint32_t getAverageCurrentMa() const { return logic_.averageCurrentMa(); }

private:
    RemainingTimeLogic logic_;
};
