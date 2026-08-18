#ifndef Button_h
#define Button_h

#include <stdint.h>
#include "../config.h"
#include "ButtonGestureLogic.h"

class Throttle;
class Sound;

// Thin wrapper over ButtonGestureLogic: reads the raw button source (physical
// pin wired, remote-forwarded state wireless — selected by
// settings.getThrottleSource()), feeds the gesture logic every check(), and
// translates the resulting intents into the throttle/sound calls. All policy
// lives in ButtonGestureLogic.h so it is host-testable.
class Button
{
    public:
        Button(uint8_t pin);
        void check();

        // Current gesture scalars, for the disarm ramp and the charge tone.
        uint8_t getPowerScale() const { return powerScale_; }
        uint8_t getArmCharge() const { return armCharge_; }

    private:
        bool readRawPressed(uint32_t nowMs);

        uint8_t pin;
        ButtonGestureLogic logic_;
        uint8_t powerScale_;
        uint8_t armCharge_;
};

#endif
