#include "config.h"
#include <mcp2515.h>
#include "Power/Power.h"
#include "SerialScreen/SerialScreen.h"

MCP2515 mcp2515(CANBUS_CS_PIN);

Buzzer buzzer(BUZZER_PIN);
Servo esc;
Throttle throttle(&buzzer);
Canbus canbus(&mcp2515);
Button button(BUTTON_PIN, &throttle, &buzzer);
Temperature motorTemp(MOTOR_TEMPERATURE_PIN);
SerialScreen screen(&throttle, &canbus, &motorTemp);

unsigned long currentLimitReachedTime = 0;
bool isCurrentLimitReached = false;
Power power(ESC_MIN_PWM, ESC_MAX_PWM);
struct can_frame canMsg;