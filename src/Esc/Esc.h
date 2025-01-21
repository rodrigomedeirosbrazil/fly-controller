#ifndef Esc_h
#define Esc_h

#include "../config.h"
#include <Servo.h>
#include "../Throttle/Throttle.h"
#include "../Canbus/Canbus.h"

class Esc {
    public:
        Esc(Canbus *canbus, Throttle *throttle);
        void handle();
        unsigned int getPowerAvailable() { return powerAvailable; }


    private:
        Servo esc;
        Canbus *canbus;
        Throttle *throttle;

        unsigned int analizeTelemetryToThrottleOutput(unsigned int throttlePercentage);
        unsigned int powerAvailable;

};
#endif
