#ifndef Throttle_h
#define Throttle_h

#include "../config.h"

class Throttle {
    public:
        Throttle();
        void handle();
        bool isArmed() { return throttleArmed; }
        void setArmed();
        void setDisarmed() { throttleArmed = false; }
        bool isCruising() { return cruising; }
        void cancelCruise() { cruising = false; }

        unsigned int getThrottlePercentage();
        unsigned int getCruisingThrottlePosition() { return cruisingThrottlePosition; }

    private:

        const unsigned int timeToBeOnCruising = 5000;
        const unsigned int throttleRange = 5;
        const unsigned int minCrusingThrottle = 30;
        const static int samples = 5;

        int pinValues[samples];
        int pinValueFiltered;
        unsigned long lastThrottleRead;

        bool throttleArmed;
        unsigned int cruising;
        unsigned int cruisingThrottlePosition;
        unsigned int lastThrottlePosition;
        unsigned long timeThrottlePosition;

        void readThrottlePin();
        void checkIfChangedCruiseState();
};

#endif