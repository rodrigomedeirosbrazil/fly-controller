#include "config.h"
#include "Power/Power.h"
#include "BatteryMonitor/BatteryMonitor.h"
#include "Xctod/Xctod.h"
#include "Settings/Settings.h"

Buzzer buzzer(BUZZER_PIN);
Servo esc;
#if IS_XAG
Throttle throttle(
    []() {
        int s = 0;
        for (int i = 0; i < 4; i++) {
            s += analogRead(THROTTLE_PIN);
        }
        return s / 4;
    }
);
#else
Throttle throttle([]() { return ads1115.readChannel(ADS1115_THROTTLE_CHANNEL); });
#endif
#if USES_CAN_BUS
Canbus canbus;
#if IS_TMOTOR
TmotorCan tmotorCan;
TmotorTelemetry tmotorTelemetry;
#else
HobbywingCan hobbywingCan;
HobbywingTelemetry hobbywingTelemetry;
#endif
twai_message_t canMsg;
#endif
Button button(BUTTON_PIN);
#if IS_XAG
BatteryVoltageSensor batterySensor(
    []() { return (int)analogRead(BATTERY_VOLTAGE_PIN); },
    BATTERY_DIVIDER_RATIO
);
Temperature motorTemp(
    []() {
        int s = 0;
        for (int i = 0; i < 4; i++) {
            s += analogRead(MOTOR_TEMPERATURE_PIN);
        }
        return s / 4;
    },
    3.3f  // ESP32 ADC reference
);
XagTelemetry xagTelemetry;
Temperature escTemp(
    []() {
        int s = 0;
        for (int i = 0; i < 4; i++) {
            s += analogRead(ESC_TEMPERATURE_PIN);
        }
        return s / 4;
    },
    3.3f  // ESP32 ADC reference
);
#else
Temperature motorTemp(
    []() { return ads1115.readChannel(ADS1115_TEMP_CHANNEL); },
    4.096f  // ADS1115 reference (GAIN_ONE)
);
#endif

Power power;
BatteryMonitor batteryMonitor;
Xctod xctod;
Settings settings;
#if IS_TMOTOR || IS_HOBBYWING
ADS1115 ads1115;
#endif
