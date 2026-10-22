#pragma once

#include <Arduino.h>
#include "BmsCapacitySyncLogic.h"

// Arduino wrapper over BmsCapacitySyncLogic: hands the BMS-reported nominal
// capacity to BatteryMonitor as the capacity in use. Never writes Settings or
// NVS. Runs on the loop task only.
class BmsCapacitySync {
public:
    void handle(bool isArmed);

private:
    BmsCapacitySyncLogic logic_;
};
