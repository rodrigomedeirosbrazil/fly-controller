#ifndef Throttle_h
#define Throttle_h

#include "../config.h"
#include "../DisarmReason.h"
#include "ThrottleCalibrationLogic.h"
#include "ThrottleDemand.h"
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

        DisarmReason getDisarmReason() const { return lastDisarmReason; }

        uint32_t getDemandPpm();
        unsigned int getThrottlePercentage();
        unsigned int getThrottleRaw();
        unsigned int getThrottlePinMin() { return throttlePinMin; }
        unsigned int getThrottlePinMax() { return throttlePinMax; }
        int getPinValueFiltered() { return pinValueFiltered; }
        unsigned int getCalibratingStep() { return calibrationLogic.step(); }

    private:
        const static int samples = 8;

        int pinValues[samples];
        int pinValueFiltered;
        ThrottleEngagementLogic engagement;
        unsigned long lastThrottleRead;

        volatile bool throttleArmed;

        bool calibrated;
        ThrottleCalibrationLogic calibrationLogic;

        unsigned int armingTries;

        int throttlePinMin;
        int throttlePinMax;

        ReadFn readFn;
        ReadOkFn readOkFn;
        bool lastSampleOk;
        ThrottleWiredValidity wiredValidity;

        ThrottleSignalLogic signalLogic;
        DisarmReason lastDisarmReason;

        void readThrottlePin();
        void resetCalibration();
        void handleCalibration(unsigned long now);
        void updateSignalValidity(unsigned long now);
};

#endif
