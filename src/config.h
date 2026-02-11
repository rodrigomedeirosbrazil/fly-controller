#ifndef CONFIG_H
#define CONFIG_H

#include "config_controller.h"
#include <ESP32Servo.h>
#include "Buzzer/Buzzer.h"
#include "Throttle/Throttle.h"
#include "Button/Button.h"
#include "Temperature/Temperature.h"
#if USES_CAN_BUS
#include "Canbus/Canbus.h"
#if IS_TMOTOR
#include "Tmotor/Tmotor.h"
#else
#include "Hobbywing/Hobbywing.h"
#endif
#include <driver/twai.h>
#endif
#include "Power/Power.h"
#include "BatteryMonitor/BatteryMonitor.h"
#include "Settings/Settings.h"
#if IS_TMOTOR || IS_HOBBYWING
#include "ADS1115/ADS1115.h"
#endif

class Power;
class Xctod;

extern Buzzer buzzer;
extern Servo esc;
extern Throttle throttle;
extern Button button;
extern Temperature motorTemp;
#if USES_CAN_BUS
extern Canbus canbus;
#if IS_TMOTOR
extern Tmotor tmotor;
#else
extern Hobbywing hobbywing;
#endif
extern twai_message_t canMsg;
#endif
#if IS_XAG
extern Temperature escTemp;
#endif
extern Power power;
extern BatteryMonitor batteryMonitor;
extern Xctod xctod;
extern Settings settings;
#if IS_TMOTOR || IS_HOBBYWING
extern ADS1115 ads1115;
#endif
// TelemetryProvider* is declared in config.cpp after including TelemetryProvider.h

// ========== ANALOG INPUTS (ADC1 - WiFi compatible) ==========
#define THROTTLE_PIN          0  // GPIO0 (ADC1-0) - Hall Sensor
#define MOTOR_TEMPERATURE_PIN 1  // GPIO1 (ADC1-1) - NTC 10K
#if IS_XAG
#define ESC_TEMPERATURE_PIN   4  // GPIO4 (ADC1-4) - NTC 10K (XAG mode only)
#define BATTERY_VOLTAGE_PIN   3  // GPIO3 - Battery voltage divider (XAG mode only) - GPIO2 isn't ADC port
#endif

// ========== DIGITAL I/O ==========
#define BUTTON_PIN 5  // GPIO5 - Push button with internal pull-up
#define BUZZER_PIN 6  // GPIO6 - Buzzer (active/passive compatible)
#define ESC_PIN    7  // GPIO7 - ESC PWM signal

// ========== CAN BUS (TWAI + SN65HVD230) ==========
#if USES_CAN_BUS
#define CAN_TX_PIN 2  // GPIO2 - Connect to SN65HVD230 CTX (TXD)
#define CAN_RX_PIN 3  // GPIO3 - Connect to SN65HVD230 CRX (RXD)
#if IS_TMOTOR
#define CAN_BITRATE TWAI_TIMING_CONFIG_1MBITS()
#else
#define CAN_BITRATE TWAI_TIMING_CONFIG_500KBITS()
#endif
#endif

// ========== BATTERY PARAMETERS ==========
#define BATTERY_CELL_COUNT 14  // 14S LiPo battery pack (fixed, not configurable)
// BATTERY_MIN_VOLTAGE and BATTERY_MAX_VOLTAGE are now managed by Settings class
#if IS_XAG
// Battery voltage divider: R1 = 2.2 MΩ, R2 = 100 kΩ
// Ratio = (R1 + R2) / R2 = (2,200,000 + 100,000) / 100,000 = 23.0
// Calibrated: actual ratio = 21.33 * (53.08V / 54.2V) = 20.90
// Low current (~26 µA), minimal noise, safe for high voltage
#define BATTERY_DIVIDER_RATIO 20.90  // Calibrated (R1 + R2) / R2
#endif

// ========== MOTOR PARAMETERS ==========
// MOTOR_MAX_TEMP and MOTOR_TEMP_REDUCTION_START are now managed by Settings class
#define MOTOR_TEMP_MIN_VALID -10000 // -10000 millicelsius = -10.000°C - Minimum valid temperature reading
#define MOTOR_TEMP_MAX_VALID 150000 // 150000 millicelsius = 150.000°C - Maximum valid temperature reading

// ========== ESC PARAMETERS ==========
#if IS_TMOTOR
#define ESC_MIN_PWM 1100
#define ESC_MAX_PWM 1940
#elif IS_XAG
#define ESC_MIN_PWM 1130
#define ESC_MAX_PWM 2000
#else
#define ESC_MIN_PWM 1050
#define ESC_MAX_PWM 1950
#endif

// ESC_MAX_TEMP and ESC_TEMP_REDUCTION_START are now managed by Settings class

#define ESC_TEMP_MIN_VALID 0 // 0 millicelsius = 0.000°C - Minimum valid temperature reading
#define ESC_TEMP_MAX_VALID 120000 // 120000 millicelsius = 120.000°C - Maximum valid temperature reading

// ========== THROTTLE RAMP LIMITING ==========
#define THROTTLE_RAMP_RATE 4 // Maximum throttle acceleration in microseconds per tick
#define THROTTLE_DECEL_MULTIPLIER 2.0 // Deceleration multiplier (deceleration is 2x faster than acceleration)

#if IS_XAG
// ========== SMOOTH START (XAG only) ==========
#define SMOOTH_START_MOTOR_STOPPED_TIME 1000  // ms - Time at ESC_MIN_PWM to consider motor stopped
#define SMOOTH_START_PRE_START_DURATION 2000 // ms - Duration of pre-start phase (5% PWM)
#define SMOOTH_START_DURATION 2000           // ms - Duration of smooth start ramp
#define SMOOTH_START_PRE_START_PERCENT 5     // % - PWM percentage during pre-start phase
#endif

// ========== ESP32-C3 ADC CONFIGURATION ==========
#define ADC_RESOLUTION 12        // 12-bit ADC (0-4095)
#define ADC_MAX_VALUE 4095       // Maximum ADC reading
#define ADC_VREF 3.3             // Reference voltage (3.3V)
#define ADC_ATTENUATION ADC_11db // Full range 0-3.3V

// ========== I2C CONFIGURATION (Tmotor and Hobbywing - ADS1115) ==========
#if IS_TMOTOR || IS_HOBBYWING
#define I2C_SDA_PIN 20           // GPIO20 - I2C SDA for ADS1115
#define I2C_SCL_PIN 21           // GPIO21 - I2C SCL for ADS1115
#define ADS1115_THROTTLE_CHANNEL 0  // ADS1115 Channel A0 - Throttle Hall Sensor
#define ADS1115_TEMP_CHANNEL 1      // ADS1115 Channel A1 - Motor NTC Sensor
#endif

#endif
