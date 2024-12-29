#ifndef Throttle_h
#define Throttle_h

#include "../config.h"

class Throttle {
    public:
        Throttle();
        void handle();
        bool isArmed() { return throttleArmed; }
        void setArmed();
        void setDisarmed();
        bool isCruising() { return cruising; }
        bool isSmoothThrottleChanging() { return smoothThrottleChanging; }
        void setCruising(int throttlePosition);
        void cancelCruise();

        unsigned int getThrottlePercentage();
        unsigned int getCruisingThrottlePosition() { return cruisingThrottlePosition; }
        void setSmoothThrottleChange(unsigned int fromThrottlePosition, unsigned int toThrottlePosition);
        void cancelSmoothThrottleChange();
        unsigned int getThrottlePercentageOnSmoothChange();
        bool isSmoothThrottleChanging() { return smoothThrottleChanging; }
        

    private:

        const unsigned int timeToBeOnCruising = 30000;
        const unsigned int throttleRange = 5;
        const unsigned int minCrusingThrottle = 30;
        const unsigned int smoothThrottleChangeSeconds = 2;
        const static int samples = 5;

        int pinValues[samples];
        int pinValueFiltered;
        unsigned long lastThrottleRead;

        bool throttleArmed;
        bool smoothThrottleChanging;
        unsigned int cruising;
        unsigned int cruisingThrottlePosition;
        unsigned int lastThrottlePosition;
        unsigned int targetThrottlePosition;
        unsigned long timeThrottlePosition;
        unsigned long timeStartSmoothThrottleChange;

        void readThrottlePin();
        void checkIfChangedCruiseState();
};

#endif