#include "config.h"
#include "Power/Power.h"
#include "Xctod/Xctod.h"

Buzzer buzzer(BUZZER_PIN);
Servo esc;
Throttle throttle;
#ifndef XAG
Canbus canbus;
Hobbywing hobbywing;
twai_message_t canMsg;
#endif
Button button(BUTTON_PIN);
Temperature motorTemp(MOTOR_TEMPERATURE_PIN);
#ifdef XAG
Temperature escTemp(ESC_TEMPERATURE_PIN);
#endif

Power power;
Xctod xctod;
