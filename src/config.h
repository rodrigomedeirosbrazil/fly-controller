#ifndef CONFIG_H
#define CONFIG_H

#include <Servo.h>
#include "Buzzer/Buzzer.h"
#include "Throttle/Throttle.h"
#include "Button/Button.h"
#include "Temperature/Temperature.h"
#include "Canbus/Canbus.h"
#include "Power/Power.h"
#include "Xctod/Xctod.h"
#include <mcp2515.h>

class Power;
class SerialScreen;
class MCP2515;

extern Buzzer buzzer;
extern Servo esc;
extern Throttle throttle;
extern Button button;
extern Temperature motorTemp;
extern SerialScreen screen;
extern Canbus canbus;
extern Power power;
extern Xctod xctod;
extern struct can_frame canMsg;
extern MCP2515 mcp2515;

#define ENABLED_CRUISE_CONTROL true

#define MOTOR_TEMPERATURE_PIN A0

#define THROTTLE_PIN A1
#define THROTTLE_PIN_MIN 170
#define THROTTLE_PIN_MAX 850
#define THROTTLE_RECOVERY_PERCENTAGE 25

#define BUTTON_PIN 3
#define BUZZER_PIN 4

#define BATTERY_MAX_CURRENT 140
#define BATTERY_CONTINUOS_CURRENT 105
#define BATTERY_MIN_VOLTAGE 462 // 46.2V in millivolts
#define BATTERY_MAX_VOLTAGE 588 // 58.8V in millivolts

#define MOTOR_MAX_TEMP 60

#define ESC_PIN 9
#define ESC_MIN_PWM 1050
#define ESC_MAX_PWM 1950

#define ESC_MAX_TEMP 110
#define ESC_MAX_CURRENT 200
#define ESC_CONTINUOS_CURRENT 80

#define CANBUS_INT_PIN 2
#define CANBUS_SCK_PIN 13
#define CANBUS_SI_PIN 11
#define CANBUS_SO_PIN 12
#define CANBUS_CS_PIN 10

#endif
