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
        void setCruising(int throttlePosition);
        void cancelCruise();

        bool isLimited() { return limited; }

        unsigned int getThrottlePosition();
        unsigned int getThrottle();
        unsigned int getCruisingThrottlePosition() { return cruisingThrottlePosition; }

    private:
        const static int samples = 5;
        const static int timeLimiting = 2000;

        int pinValues[samples];
        int pinValueFiltered;
        unsigned long lastThrottleRead;

        bool throttleArmed;
        unsigned int cruising;
        unsigned int cruisingThrottlePosition;

        unsigned int lastThrottlePosition;
        bool limited;
        unsigned int thresholdToLimit;

        void readThrottlePin();
        unsigned int handleLimited();
        void setLimiting(unsigned int throttlePosition, unsigned int threshold);
};

#endif
