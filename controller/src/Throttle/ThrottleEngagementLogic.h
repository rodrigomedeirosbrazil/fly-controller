#pragma once

// Pure throttle engage/release hysteresis logic — no Arduino deps, host-testable.
//
// Protects against Hall-sensor drift/noise at idle: the motor only engages once the
// filtered reading clears ENGAGE_THRESHOLD_PERCENT of the calibrated range above
// pinMin, and releases only after dropping below RELEASE_THRESHOLD_PERCENT (lower
// than the engage threshold) to avoid chatter around the boundary.
//
// Calibration (throttlePinMin/throttlePinMax) runs on every boot and is not
// persisted, so pinMin is always a fresh at-rest reading from just before the
// engage threshold is evaluated — no separate arm-time re-zero is needed.
class ThrottleEngagementLogic {
public:
    ThrottleEngagementLogic() : engaged_(false) {}

    bool update(int filtered, int pinMin, int pinMax) {
        int range = pinMax - pinMin;
        if (range <= 0) {
            engaged_ = false;
            return engaged_;
        }

        int engageThreshold  = pinMin + (range * ENGAGE_THRESHOLD_PERCENT) / 100;
        int releaseThreshold = pinMin + (range * RELEASE_THRESHOLD_PERCENT) / 100;

        if (!engaged_ && filtered > engageThreshold) {
            engaged_ = true;
        } else if (engaged_ && filtered < releaseThreshold) {
            engaged_ = false;
        }

        return engaged_;
    }

    bool isEngaged() const { return engaged_; }
    void reset() { engaged_ = false; }

private:
    static const int ENGAGE_THRESHOLD_PERCENT  = 2;
    static const int RELEASE_THRESHOLD_PERCENT = 1;

    bool engaged_;
};
