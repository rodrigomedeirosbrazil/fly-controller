#ifndef CONFIG_H
#define CONFIG_H

#include <ESP32Servo.h>
#include "Buzzer/Buzzer.h"
#include "Throttle/Throttle.h"
#include "Button/Button.h"
#include "Temperature/Temperature.h"
#ifndef XAG
#include "Canbus/Canbus.h"
#include "Hobbywing/Hobbywing.h"
#include <driver/twai.h>
#endif
#include "Power/Power.h"

class Power;
class Xctod;

extern Buzzer buzzer;
extern Servo esc;
extern Throttle throttle;
extern Button button;
extern Temperature motorTemp;
#ifndef XAG
extern Canbus canbus;
extern Hobbywing hobbywing;
extern twai_message_t canMsg;
#endif
#ifdef XAG
extern Temperature escTemp;
#endif
extern Power power;
extern Xctod xctod;

// ========== ANALOG INPUTS (ADC1 - WiFi compatible) ==========
#define THROTTLE_PIN          0  // GPIO0 (ADC1-0) - Hall Sensor
#define MOTOR_TEMPERATURE_PIN 1  // GPIO1 (ADC1-1) - NTC 10K
#ifdef XAG
#define ESC_TEMPERATURE_PIN   4  // GPIO4 (ADC1-4) - NTC 10K (XAG mode only)
#define BATTERY_VOLTAGE_PIN   2  // GPIO2 (ADC1-2) - Battery voltage divider (XAG mode only)
#endif

// ========== DIGITAL I/O ==========
#define BUTTON_PIN 5  // GPIO5 - Push button with internal pull-up
#define BUZZER_PIN 6  // GPIO6 - Buzzer (active/passive compatible)
#define ESC_PIN    7  // GPIO7 - ESC PWM signal

// ========== CAN BUS (TWAI + SN65HVD230) ==========
#ifndef XAG
#define CAN_TX_PIN 2  // GPIO2 - Connect to SN65HVD230 CTX (TXD)
#define CAN_RX_PIN 3  // GPIO3 - Connect to SN65HVD230 CRX (RXD)
#define CAN_BITRATE TWAI_TIMING_CONFIG_500KBITS()
#endif

// ========== BATTERY PARAMETERS ==========
#define BATTERY_MIN_VOLTAGE 441 // 441 decivolts = 44.1 V - ~3.15 V per cell
#define BATTERY_MAX_VOLTAGE 585 //  585 decivolts = 58.1 V - 4.15 V per cell
#ifdef XAG
// Battery voltage divider: R1 = 2.2 MΩ, R2 = 100 kΩ
// Ratio = (R1 + R2) / R2 = (2,200,000 + 100,000) / 100,000 = 23.0
// Calibrated: actual ratio = 23.0 * (53.41V / 75.90V) = 16.19
// Low current (~26 µA), minimal noise, safe for high voltage
#define BATTERY_DIVIDER_RATIO 16.19  // Calibrated (R1 + R2) / R2
#endif

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

// ========== ESP32-C3 ADC CONFIGURATION ==========
#define ADC_RESOLUTION 12        // 12-bit ADC (0-4095)
#define ADC_MAX_VALUE 4095       // Maximum ADC reading
#define ADC_VREF 3.3             // Reference voltage (3.3V)
#define ADC_ATTENUATION ADC_11db // Full range 0-3.3V

#endif
