#ifndef Button_h
#define Button_h

#include <AceButton.h>
#include "../config.h"

class Throttle;
class Buzzer;

using namespace ace_button;

// Reads the physical pin in wired mode, or the remote's forwarded button
// state in wireless mode — so the same AceButton arming gesture works for both.
class SourceSwitchButtonConfig : public ButtonConfig {
  protected:
    int readButton(uint8_t pin) override;
};

class Button
{
    public:
        Button(uint8_t pin);
        void check();
        void handleEvent(AceButton* aceButton, uint8_t eventType, uint8_t buttonState);

    private:
        const static unsigned long longClickThreshold = 3500;

        AceButton aceButton;
        SourceSwitchButtonConfig sourceConfig;
        uint8_t pin;
        ButtonConfig* buttonConfig;
        unsigned long releaseButtonTime;
        bool buttonWasClicked;
};

#endif
