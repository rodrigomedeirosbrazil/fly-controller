#ifndef Button_h
#define Button_h

#include <AceButton.h>
#include "../config.h"

class Throttle;
class Buzzer;

using namespace ace_button;

class Button
{
    public:
        Button(uint8_t pin, Throttle *throttle, Buzzer* buzzer);
        void check();
        void handleEvent(AceButton* aceButton, uint8_t eventType, uint8_t buttonState);

    private:
        const static unsigned long longClickThreshold = 3500;

        AceButton aceButton;
        Throttle *throttle;
        Buzzer *buzzer;
        uint8_t pin;
        ButtonConfig* buttonConfig;
        unsigned long releaseButtonTime;
        bool buttonWasClicked;
};

#endif
