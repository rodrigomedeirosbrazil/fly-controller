#ifndef Screen_h
#define Screen_h

#include "../Display/Display.h"
#include "../Throttle/Throttle.h"
#include "../Canbus/Canbus.h"
#include "../Temperature/Temperature.h"

static const unsigned char image_voltage_bits[] U8X8_PROGMEM = {0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x03, 0x80, 0x01, 0xc0, 0x01, 0xe0, 0x00, 0xf0, 0x07, 0x80, 0x03, 0xc0, 0x01, 0xc0, 0x00, 0x60, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const unsigned char image_refresh_bits[] U8X8_PROGMEM = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x98, 0x07, 0x84, 0x03, 0x82, 0x03, 0x81, 0x04, 0x01, 0x04, 0x01, 0x04, 0x01, 0x04, 0x01, 0x04, 0x02, 0x02, 0x04, 0x01, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00};

class Screen
{
    public:
        Screen(
            Display* display,
            Throttle* throttle,
            Canbus* canbus,
            Temperature* motorTemp
        );
        void draw();
        void drawUi();

    private:
        Display* display;
        Throttle* throttle;
        Canbus* canbus;
        Temperature* motorTemp;
        
        unsigned long lastScreenUpdate;

        void drawMotorTemp();
        void drawCurrent();
        void drawEscInfo();
        void drawRpm();
        void drawThrottleBar();
        void drawArmed();
        void drawCruise();
        void drawBatteryBar();

};

#endif