#include "config.h"
#include "PowerAlert/PowerAlert.h"
#include "Power/Power.h"
#include "BatteryMonitor/BatteryMonitor.h"
#include "BluetoothBms/BluetoothBms.h"
#include "DalyBms/DalyBms.h"
#include "Xctod/Xctod.h"
#include "TelemetryLogger/TelemetryLogger.h"
#include "JbdBms/JbdBms.h"
#include "JkBms/JkBms.h"
#include "Settings/Settings.h"

Buzzer buzzer(BUZZER_PIN);
Servo esc;
Throttle throttle([]() -> int {
    if (settings.getThrottleSource() == ThrottleSourceWireless) {
        // Failsafe ramp: feed 0 so the existing ramp-down drives the motor to idle.
        if (remoteLink.failsafe(true, millis()) != FailsafeAction::None) return 0;
        return (int)remoteLink.lastHallRaw();
    }
    return ads1115.readChannel(ADS1115_THROTTLE_CHANNEL);
});
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
PowerAlert powerAlert;
BatteryMonitor batteryMonitor;
BluetoothBms bluetoothBms;
DalyBms dalyBms;
Xctod xctod;
TelemetryLogger telemetryLogger;
JbdBms jbdBms;
JkBms jkBms;
Settings settings;
HourMeter hourMeter;
ADS1115 ads1115;
RemoteLink remoteLink;
