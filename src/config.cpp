#include "config.h"
#include "Power/Power.h"
#include "Xctod/Xctod.h"

Buzzer buzzer(BUZZER_PIN);
Servo esc;
Throttle throttle;
Canbus canbus;
Hobbywing hobbywing;
Jkbms jkbms;
Button button(BUTTON_PIN);
Temperature motorTemp(MOTOR_TEMPERATURE_PIN);

Power power;
Xctod xctod;
twai_message_t canMsg;