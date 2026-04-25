#include "HourMeter.h"

static const char NVS_NAMESPACE[] = "hrMeter";
static const char NVS_KEY[] = "hrMeterSec";
static const unsigned long SAVE_INTERVAL_MS = 60000;

void HourMeter::init() {
    prefs_.begin(NVS_NAMESPACE, false);
    hourMeterSec_ = prefs_.getUInt(NVS_KEY, 0);
    lastNvsSaveMs_ = millis();
}

void HourMeter::handle(bool isArmed, bool motorRunning) {
    unsigned long now = millis();

    // Session timer
    if (isArmed && !wasArmed_) {
        sessionStartMs_ = now;
        sessionSec_ = 0;
    }
    if (isArmed) {
        sessionSec_ = (uint32_t)((now - sessionStartMs_) / 1000);
    }

    // Hour meter — motor started
    if (motorRunning && !wasMotorRunning_) {
        motorRunStartMs_ = now;
    }

    // Hour meter — motor stopped
    if (!motorRunning && wasMotorRunning_) {
        hourMeterSec_ += (uint32_t)((now - motorRunStartMs_) / 1000);
        motorRunStartMs_ = 0;
        saveToNvs();
    }

    // Disarm — flush and save
    if (!isArmed && wasArmed_) {
        if (wasMotorRunning_) {
            hourMeterSec_ += (uint32_t)((now - motorRunStartMs_) / 1000);
            motorRunStartMs_ = 0;
        }
        saveToNvs();
    }

    // Periodic save every 60s while motor is running
    if (motorRunning && (now - lastNvsSaveMs_) >= SAVE_INTERVAL_MS) {
        uint32_t liveValue = hourMeterSec_ + (uint32_t)((now - motorRunStartMs_) / 1000);
        prefs_.putUInt(NVS_KEY, liveValue);
        lastNvsSaveMs_ = now;
    }

    wasArmed_ = isArmed;
    wasMotorRunning_ = motorRunning;
}

uint32_t HourMeter::getHourMeterSec() const {
    if (motorRunStartMs_ != 0) {
        return hourMeterSec_ + (uint32_t)((millis() - motorRunStartMs_) / 1000);
    }
    return hourMeterSec_;
}

uint32_t HourMeter::getSessionSec() const {
    return sessionSec_;
}

void HourMeter::saveToNvs() {
    prefs_.putUInt(NVS_KEY, hourMeterSec_);
    lastNvsSaveMs_ = millis();
}
