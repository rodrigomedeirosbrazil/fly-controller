#include "config.h"
#include "Power/Power.h"
#include "BatteryMonitor/BatteryMonitor.h"
#include "BluetoothBms/BluetoothBms.h"
#include "Xctod/Xctod.h"
#include "JbdBms/JbdBms.h"
#include "Settings/Settings.h"

Buzzer buzzer(BUZZER_PIN);
Servo esc;
Throttle throttle([]() { return ads1115.readChannel(ADS1115_THROTTLE_CHANNEL); });
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
    []() { return ads1115.readChannel(ADS1115_BATTERY_CHANNEL); },
    BATTERY_DIVIDER_RATIO,
    4.096f  // ADS1115 reference (GAIN_ONE)
);
Temperature motorTemp(
    []() { return ads1115.readChannel(ADS1115_MOTOR_TEMP_CHANNEL); },
    4.096f  // ADS1115 reference (GAIN_ONE)
);
XagTelemetry xagTelemetry;
Temperature escTemp(
    []() { return ads1115.readChannel(ADS1115_ESC_TEMP_CHANNEL); },
    4.096f  // ADS1115 reference (GAIN_ONE)
);
#elif IS_TMOTOR
BatteryVoltageSensor batterySensor(
    []() { return ads1115.readChannel(ADS1115_BATTERY_CHANNEL); },
    BATTERY_DIVIDER_RATIO,
    4.096f  // ADS1115 reference (GAIN_ONE)
);
Temperature motorTemp(
    []() { return ads1115.readChannel(ADS1115_MOTOR_TEMP_CHANNEL); },
    4.096f  // ADS1115 reference (GAIN_ONE)
);
#else
Temperature motorTemp(
    []() { return ads1115.readChannel(ADS1115_MOTOR_TEMP_CHANNEL); },
    4.096f  // ADS1115 reference (GAIN_ONE)
);
#endif

Power power;
BatteryMonitor batteryMonitor;
BluetoothBms bluetoothBms;
Xctod xctod;
JbdBms jbdBms;
Settings settings;
ADS1115 ads1115;
