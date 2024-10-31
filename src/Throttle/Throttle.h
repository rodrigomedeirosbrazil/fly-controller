#ifndef Throttle_h
#define Throttle_h

#include "../PwmReader/PwmReader.h"

class Throttle {
    public:
        Throttle(PwmReader *pwmReader);
        void tick();
        bool isArmed() { return throttleArmed; }
        bool isCruising() { return cruising; }
        void cancelCruise() { cruising = false; timeThrottlePosition = millis(); }

        unsigned int getThrottlePercentageFiltered();
        unsigned int getCruisingThrottlePosition() { return cruisingThrottlePosition; }
        unsigned long getCruisingStartTime() { return cruisingStartTime; }

    private:
        PwmReader *pwmReader;

        bool throttleArmed;
        unsigned long throttleFullReverseTime;
        bool throttleFullReverseFirstTime;
        unsigned long timeThrottlePosition;
        int lastThrottlePosition;
        unsigned int cruising;
        unsigned int cruisingThrottlePosition;
        unsigned long cruisingStartTime;

        void checkIfChangedArmedState(int throttlePercentage, unsigned int now);
        void checkIfChangedCruiseState(int throttlePercentage, unsigned int now);
};

#endif