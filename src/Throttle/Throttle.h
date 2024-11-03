#ifndef Throttle_h
#define Throttle_h

#include "../config.h"

class Throttle {
    public:
        Throttle();
        void tick();
        bool isArmed() { return throttleArmed; }
        bool isCruising() { return cruising; }
        void cancelCruise() { cruising = false; timeThrottlePosition = millis(); }

        unsigned int getThrottlePercentage();
        unsigned int getCruisingThrottlePosition() { return cruisingThrottlePosition; }
        unsigned long getCruisingStartTime() { return cruisingStartTime; }

    private:

        int throttlePinValues[SAMPLES_FOR_FILTER];
        int throttlePinValueFiltered;
        unsigned long lastThrottleRead;

        bool throttleArmed;
        unsigned long throttleFullReverseTime;
        bool throttleFullReverseFirstTime;
        unsigned long timeThrottlePosition;
        int lastThrottlePosition;
        unsigned int cruising;
        unsigned int cruisingThrottlePosition;
        unsigned long cruisingStartTime;

        void readThrottlePin();
        void checkIfChangedCruiseState(int throttlePercentage, unsigned int now);
};

#endif