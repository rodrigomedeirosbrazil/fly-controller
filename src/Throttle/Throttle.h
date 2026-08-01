#ifndef Throttle_h
#define Throttle_h

#include "../Buzzer/Buzzer.h"
#include "../config.h"
#include "../DisarmReason.h"
#include "ThrottleEngagementLogic.h"
#include "ThrottleSignalLogic.h"
#include "ThrottleWiredValidity.h"

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

        // True whenever ThrottleSignalLogic's last action was ForceZero or
        // Disarm. Wireless can spend real time here pre-disarm (up to
        // disarmMs) and stays true afterward too, for as long as it remains
        // disarmed. Wired's ThrottleSignalLogic action is always Disarm, never
        // ForceZero (its disarmMs is 0), but this flag is set on Disarm as
        // well and — same as wireless — stays true for as long as it remains
        // disarmed, not just for a single tick. It has no consumer outside
        // this class; wired's readThrottlePin() never acts on it (see the
        // comment there for why that matters).
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
        ThrottleWiredValidity wiredValidity;

        ThrottleSignalLogic signalLogic;
        bool signalForcedZero;
        DisarmReason lastDisarmReason;
        bool wasWireless;

        void readThrottlePin();
        void resetCalibration();
        void handleCalibration(unsigned long now);
        void updateSignalValidity(unsigned long now);
};

#endif
