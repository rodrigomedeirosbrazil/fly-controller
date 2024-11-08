#ifndef Button_h
#define Button_h

#include <AceButton.h>

#include "../config.h"
#include "../Throttle/Throttle.h"

using namespace ace_button;

class Button : public IEventHandler
{
    public:
        Button(uint8_t pin, Throttle *throttle);
        void check();

    private:
        const static unsigned long longClickThreshold = 3500;

        AceButton aceButton;
        Throttle *throttle;
        uint8_t pin;
        unsigned long releaseButtonTime;
        bool buttonWasClicked;

        void handleEvent(AceButton* aceButton, uint8_t eventType, uint8_t buttonState);
};

#endif