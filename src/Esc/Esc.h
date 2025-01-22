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
        void setPowerAvailable(unsigned int power);

    private:
        const static unsigned long minTimeToChangePowerAvailable = 1000;

        Servo esc;
        Canbus *canbus;
        Throttle *throttle;

        unsigned int powerAvailable;
        unsigned long lastPowerAvailableChange;
};
#endif
