#ifndef CONFIG_H
#define CONFIG_H

#include <Servo.h>
#include "Buzzer/Buzzer.h"
#include "Throttle/Throttle.h"
#include "Button/Button.h"
#include "Temperature/Temperature.h"
#include "Canbus/Canbus.h"
#include "Hobbywing/Hobbywing.h"
#include "Jkbms/Jkbms.h"
#include "Power/Power.h"

// ESP32-C3 TWAI (CAN) driver
#include <driver/twai.h>

class Power;
class Xctod;

extern Buzzer buzzer;
extern Servo esc;
extern Throttle throttle;
extern Button button;
extern Temperature motorTemp;
extern Canbus canbus;
extern Hobbywing hobbywing;
extern Jkbms jkbms;
extern Power power;
extern Xctod xctod;
extern twai_message_t canMsg;

// ========== ANALOG INPUTS (ADC1 - WiFi compatible) ==========
#define THROTTLE_PIN          0  // GPIO0 (ADC1-0) - Hall Sensor
#define MOTOR_TEMPERATURE_PIN 1  // GPIO1 (ADC1-1) - NTC 10K

// Legacy compatibility values (will be auto-calibrated)
#define THROTTLE_PIN_MIN 170
#define THROTTLE_PIN_MAX 850
#define THROTTLE_RECOVERY_PERCENTAGE 25

// ========== DIGITAL I/O ==========
#define BUTTON_PIN 5  // GPIO5 - Push button with internal pull-up
#define BUZZER_PIN 6  // GPIO6 - Buzzer (active/passive compatible)
#define ESC_PIN    7  // GPIO7 - ESC PWM signal

// ========== CAN BUS (TWAI + SN65HVD230) ==========
#define CAN_TX_PIN 2  // GPIO2 - Connect to SN65HVD230 CTX (TXD)
#define CAN_RX_PIN 3  // GPIO3 - Connect to SN65HVD230 CRX (RXD)
#define CAN_BITRATE TWAI_TIMING_CONFIG_500KBITS()

// ========== BATTERY PARAMETERS ==========
#define BATTERY_MAX_CURRENT 140
#define BATTERY_CONTINUOS_CURRENT 105
#define BATTERY_MIN_VOLTAGE 420 // 420 decivolts = 42 V - 3.0 V per cell
#define BATTERY_MAX_VOLTAGE 574 //  574 decivolts = 57.4 V - 4.1 V per cell

// ========== MOTOR PARAMETERS ==========
#define MOTOR_MAX_TEMP 60
#define MOTOR_TEMP_REDUCTION_START 50 // Start reducing power at this temperature
#define MOTOR_TEMP_MIN_VALID -10 // Minimum valid temperature reading
#define MOTOR_TEMP_MAX_VALID 150 // Maximum valid temperature reading

// ========== ESC PARAMETERS ==========
#define ESC_MIN_PWM 1050
#define ESC_MAX_PWM 1950
#define ESC_MAX_TEMP 110
#define ESC_TEMP_REDUCTION_START 80 // Start reducing power at this temperature
#define ESC_TEMP_MIN_VALID 0 // Minimum valid temperature reading
#define ESC_TEMP_MAX_VALID 120 // Maximum valid temperature reading
#define ESC_MAX_CURRENT 200
#define ESC_CONTINUOS_CURRENT 80

// ========== ESP32-C3 ADC CONFIGURATION ==========
#define ADC_RESOLUTION 12        // 12-bit ADC (0-4095)
#define ADC_MAX_VALUE 4095       // Maximum ADC reading
#define ADC_VREF 3.3             // Reference voltage (3.3V)
#define ADC_ATTENUATION ADC_11db // Full range 0-3.3V

#endif
