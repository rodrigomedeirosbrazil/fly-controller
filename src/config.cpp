#include "config.h"
#include <mcp2515.h>
#include "Power/Power.h"
#include "SerialScreen/SerialScreen.h"

MCP2515 mcp2515(CANBUS_CS_PIN);

Buzzer buzzer(BUZZER_PIN);
Servo esc;
Throttle throttle;
Canbus canbus;
Button button(BUTTON_PIN);
Temperature motorTemp(MOTOR_TEMPERATURE_PIN);
SerialScreen screen;

Power power;
struct can_frame canMsg;