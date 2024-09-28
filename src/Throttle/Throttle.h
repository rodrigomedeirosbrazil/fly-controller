#ifndef Throttle_h
#define Throttle_h

class Throttle {
    public:
        Throttle();
        void tick(int throttlePercentage, unsigned int now);
        bool isArmed() { return throttleArmed; }
        bool isCruising() { return cruising; }
        unsigned int getCruisingThrottlePosition() { return cruisingThrottlePosition; }
        unsigned long getCruisingStartTime() { return cruisingStartTime; }
        unsigned int getThrottlePercentageFiltered(int throttlePercentage);

    private:
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