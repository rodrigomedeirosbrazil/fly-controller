#pragma once

#include <Arduino.h>
#include <Preferences.h>

class HourMeter {
public:
    void init();
    void handle(bool isArmed, bool motorRunning);
    uint32_t getHourMeterSec() const;
    uint32_t getSessionSec() const;

private:
    Preferences prefs_;
    uint32_t hourMeterSec_ = 0;
    uint32_t sessionSec_ = 0;
    unsigned long motorRunStartMs_ = 0;
    unsigned long sessionStartMs_ = 0;
    unsigned long lastNvsSaveMs_ = 0;
    bool wasArmed_ = false;
    bool wasMotorRunning_ = false;

    void saveToNvs();
};
