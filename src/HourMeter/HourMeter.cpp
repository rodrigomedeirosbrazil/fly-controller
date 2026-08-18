#include "HourMeter.h"

static const char NVS_NAMESPACE[] = "hrMeter";
static const char NVS_KEY[] = "hrMeterSec";
static const unsigned long SAVE_INTERVAL_MS = 60000;

void HourMeter::init() {
    prefs_.begin(NVS_NAMESPACE, false);
    logic_.hourMeterSec = prefs_.getUInt(NVS_KEY, 0);
    lastNvsSaveMs_ = millis();
}

void HourMeter::handle(bool isArmed, bool motorRunning) {
    unsigned long now = millis();
    logic_.tick(isArmed, motorRunning, now);

    // Flush the persistent total on motor stop or disarm — logic_ has just
    // folded the running interval into hourMeterSec.
    if ((wasMotorRunning_ && !motorRunning) || (wasArmed_ && !isArmed)) {
        saveToNvs();
    }

    // Periodic save while the motor runs, so a power loss loses at most 60 s.
    if (motorRunning && (now - lastNvsSaveMs_) >= SAVE_INTERVAL_MS) {
        saveToNvs();
    }

    wasArmed_ = isArmed;
    wasMotorRunning_ = motorRunning;
}

void HourMeter::requestReset() {
    logic_.requestReset();
}

uint32_t HourMeter::getHourMeterSec() const {
    return logic_.getHourMeterSec(millis());
}

uint32_t HourMeter::getSessionSec() const {
    return logic_.getSessionSec(millis());
}

void HourMeter::saveToNvs() {
    prefs_.putUInt(NVS_KEY, logic_.getHourMeterSec(millis()));
    lastNvsSaveMs_ = millis();
}
