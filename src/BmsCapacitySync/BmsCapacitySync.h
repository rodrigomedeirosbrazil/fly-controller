#pragma once

#include <Arduino.h>
#include "BmsCapacitySyncLogic.h"

// Arduino wrapper over BmsCapacitySyncLogic: adopts the BMS-reported nominal
// capacity as the battery capacity setting. Runs on the loop task only.
class BmsCapacitySync {
public:
    void handle(bool isArmed);

private:
    BmsCapacitySyncLogic logic_;
};
