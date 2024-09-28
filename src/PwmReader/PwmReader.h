#ifndef PwmReader_h
#define PwmReader_h

#include "../config.h"

#define NUM_READINGS 16

class PwmReader {
    public:
        PwmReader();
        void tick(int rawThrottlePercentage);
        int getThrottlePercentage() { return throttlePercentage; }

    private:
        int rawReadings[NUM_READINGS];
        int throttlePercentage;

        void initRawReadings();
        int getAverageReading(int newRead);
};

#endif