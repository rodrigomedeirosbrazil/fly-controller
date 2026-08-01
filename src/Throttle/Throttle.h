#ifndef Throttle_h
#define Throttle_h

#include "../Buzzer/Buzzer.h"
#include "../config.h"
#include "../DisarmReason.h"
#include "ThrottleEngagementLogic.h"
#include "ThrottleSignalLogic.h"

class Throttle {
    public:
        typedef int (*ReadFn)();
        typedef bool (*ReadOkFn)();
        Throttle(ReadFn readFn, ReadOkFn readOkFn);
        void handle();
        bool isArmed() { return throttleArmed; }
        void setArmed();
        void setDisarmed(DisarmReason reason = DisarmReason::Manual);
        bool isCalibrated() { return calibrated; }
        bool isEngaged() { return engagement.isEngaged(); }

        // True while an invalid throttle signal is being held at zero before
        // the disarm threshold is reached. Wireless can spend real time here
        // (up to disarmMs); wired passes through it only for the single tick
        // that triggers Disarm, since its own disarmMs is 0.
        bool isSignalForcedZero() const { return signalForcedZero; }
        DisarmReason getDisarmReason() const { return lastDisarmReason; }

        unsigned int getThrottlePercentage();
        unsigned int getThrottleRaw();
        unsigned int getThrottlePinMin() { return throttlePinMin; }
        unsigned int getThrottlePinMax() { return throttlePinMax; }
        int getPinValueFiltered() { return pinValueFiltered; }
        unsigned int getCalibratingStep() { return calibratingStep; }

    private:
        const static int samples = 8;
        const unsigned int calibrationTime = 3000; // 3 seconds for calibration
        const int calibrationThreshold = 2000; // Threshold for detecting throttle movement
        const static int WIRED_BAND_MARGIN_LOW_PERCENT = 20;
        const static int WIRED_BAND_MARGIN_HIGH_PERCENT = 10;
        const static unsigned int WIRED_I2C_FAIL_STREAK_THRESHOLD = 3;

        int pinValues[samples];
        int pinValueFiltered;
        ThrottleEngagementLogic engagement;
        unsigned long lastThrottleRead;

        volatile bool throttleArmed;

        bool calibrated;
        unsigned int calibratingStep;
        unsigned long calibrationStartTime;
        int calibrationMaxValue;
        int calibrationMinValue;
        unsigned long calibrationSumMax;
        unsigned int calibrationCountMax;
        unsigned long calibrationSumMin;
        unsigned int calibrationCountMin;

        unsigned int armingTries;

        int throttlePinMin;
        int throttlePinMax;

        ReadFn readFn;
        ReadOkFn readOkFn;
        bool lastSampleOk;
        unsigned int i2cFailStreak;

        ThrottleSignalLogic signalLogic;
        bool signalForcedZero;
        DisarmReason lastDisarmReason;
        bool wasWireless;

        static constexpr ThrottleSignalConfig kWiredSignalConfig{0, 0, 0};
        static constexpr ThrottleSignalConfig kWirelessSignalConfig{500, 3000, 200};

        void readThrottlePin();
        void resetCalibration();
        void handleCalibration(unsigned long now);
        void updateSignalValidity(unsigned long now);
};

#endif
