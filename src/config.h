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
#include "Tmotor/TmotorCan.h"
#include "Tmotor/TmotorTelemetry.h"
#else
#include "Hobbywing/HobbywingCan.h"
#include "Hobbywing/HobbywingTelemetry.h"
#endif
#include <driver/twai.h>
#endif
#include "Power/Power.h"
#include "BatteryMonitor/BatteryMonitor.h"
#include "Settings/Settings.h"
#include "ADS1115/ADS1115.h"
#if IS_XAG
#include "Sensors/BatteryVoltageSensor.h"
#include "Xag/XagTelemetry.h"
#endif

// Debug logging - compiles to zero in production
#ifdef DEBUG
    #define DEBUG_PRINT(x)       Serial.print(x)
    #define DEBUG_PRINTLN(x)     Serial.println(x)
    #define DEBUG_PRINT_HEX(x,b) Serial.print(x, b)
#else
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINTLN(x)
    #define DEBUG_PRINT_HEX(x,b)
#endif

class Power;
class Xctod;
class JbdBms;

extern Buzzer buzzer;
extern Servo esc;
extern Throttle throttle;
extern Button button;
extern Temperature motorTemp;
#if USES_CAN_BUS
extern Canbus canbus;
#if IS_TMOTOR
extern TmotorCan tmotorCan;
extern TmotorTelemetry tmotorTelemetry;
#else
extern HobbywingCan hobbywingCan;
extern HobbywingTelemetry hobbywingTelemetry;
#endif
extern twai_message_t canMsg;
#endif
#if IS_XAG
extern Temperature escTemp;
extern BatteryVoltageSensor batterySensor;
extern XagTelemetry xagTelemetry;
#endif
extern Power power;
extern BatteryMonitor batteryMonitor;
extern Xctod xctod;
extern JbdBms jbdBms;
extern Settings settings;
extern ADS1115 ads1115;
#include "Telemetry/Telemetry.h"

// ========== ANALOG INPUTS (ADC1 - legacy, used only when ADS1115 not in use) ==========
#define THROTTLE_PIN          0  // GPIO0 (ADC1-0) - Hall Sensor
#define MOTOR_TEMPERATURE_PIN 1  // GPIO1 (ADC1-1) - NTC 10K

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
// Internal reserve: usable capacity as % of nominal (not exposed to users)
#define BATTERY_USABLE_PERCENT 80
// Recalibrate coulomb count when current stays below threshold for a while.
#define BATTERY_RECALIBRATION_CURRENT_MA 200
#define BATTERY_RECALIBRATION_STABLE_MS 2000
// BATTERY_MIN_VOLTAGE and BATTERY_MAX_VOLTAGE are now managed by Settings class
#if IS_XAG
// Battery voltage divider: R1 = 2.2 MΩ, R2 = 100 kΩ
// Ratio = (R1 + R2) / R2 = (2,200,000 + 100,000) / 100,000 = 23.0
// Calibrated: 20.90 * (51.45V / 46.493V) = 23.13 (corrected under-reading)
// Low current (~26 µA), minimal noise, safe for high voltage
#define BATTERY_DIVIDER_RATIO 23.13  // Calibrated (R1 + R2) / R2
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
#define THROTTLE_RAMP_UP_US_PER_MS     1.0f
#define THROTTLE_RAMP_DOWN_US_PER_MS   4.0f
#define THROTTLE_DEADBAND_US           20
#define MOTOR_STOP_TIME_MS             800

// XAG motor (useSmoothStart): 1.5s reaction delay when stopped. Send 5% PWM (wake-up) during that time; after 1.5s, ramp from 5% to target.
// Only used when useSmoothStart is true (XAG build).
#define XAG_MOTOR_REACTION_DELAY_MS    1500
#define XAG_WAKEUP_PWM_PERCENT        5

// ========== ESP32-C3 ADC CONFIGURATION ==========
#define ADC_RESOLUTION 12        // 12-bit ADC (0-4095)
#define ADC_MAX_VALUE 4095       // Maximum ADC reading
#define ADC_VREF 3.3             // Reference voltage (3.3V)
#define ADC_ATTENUATION ADC_11db // Full range 0-3.3V

// ========== I2C CONFIGURATION (ADS1115 - all builds) ==========
#define I2C_SDA_PIN 20             // GPIO20 - I2C SDA for ADS1115
#define I2C_SCL_PIN 21             // GPIO21 - I2C SCL for ADS1115
#define ADS1115_THROTTLE_CHANNEL   0  // ADS1115 Channel A0 - Throttle Hall Sensor
#define ADS1115_MOTOR_TEMP_CHANNEL 1  // ADS1115 Channel A1 - Motor NTC Sensor
#define ADS1115_ESC_TEMP_CHANNEL   2  // ADS1115 Channel A2 - ESC NTC (XAG only)
#define ADS1115_BATTERY_CHANNEL    3  // ADS1115 Channel A3 - Battery voltage divider (XAG only)

#endif
