#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "HourMeterLogic.h"

class HourMeter {
public:
    void init();
    void handle(bool isArmed, bool motorRunning);
    void requestReset();
    uint32_t getHourMeterSec() const;
    uint32_t getSessionSec() const;

private:
    Preferences prefs_;
    unsigned long lastNvsSaveMs_ = 0;
    bool wasArmed_ = false;
    bool wasMotorRunning_ = false;
    HourMeterLogic logic_;

    void saveToNvs();
};
