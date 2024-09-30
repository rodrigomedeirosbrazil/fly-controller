#ifndef PwmReader_h
#define PwmReader_h

#include "../config.h"

class PwmReader {
    public:
        PwmReader();
        void tick(int rawThrottlePercentage);
        int getThrottlePercentage() { return throttlePercentage; }

    private:
        int throttlePercentage;
};

#endif