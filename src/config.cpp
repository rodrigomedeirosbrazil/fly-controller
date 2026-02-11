#include "config.h"
#include "Telemetry/TelemetryProvider.h"
#include "Power/Power.h"
#include "BatteryMonitor/BatteryMonitor.h"
#include "Xctod/Xctod.h"
#include "Settings/Settings.h"

// Select telemetry provider based on build configuration
#if IS_XAG
    #include "Telemetry/XagProvider.h"
    static TelemetryProvider telemetryProvider = createXagProvider();
#elif IS_TMOTOR
    #include "Telemetry/TmotorProvider.h"
    static TelemetryProvider telemetryProvider = createTmotorProvider();
#else
    #include "Telemetry/HobbywingProvider.h"
    static TelemetryProvider telemetryProvider = createHobbywingProvider();
#endif

Buzzer buzzer(BUZZER_PIN);
Servo esc;
Throttle throttle;
#if USES_CAN_BUS
Canbus canbus;
#if IS_TMOTOR
Tmotor tmotor;
#else
Hobbywing hobbywing;
#endif
twai_message_t canMsg;
#endif
Button button(BUTTON_PIN);
Temperature motorTemp(MOTOR_TEMPERATURE_PIN);
#if IS_XAG
Temperature escTemp(ESC_TEMPERATURE_PIN);
#endif

Power power;
BatteryMonitor batteryMonitor;
Xctod xctod;
Settings settings;
#if IS_TMOTOR || IS_HOBBYWING
ADS1115 ads1115;
#endif

// Define telemetry pointer (declared above after includes)
TelemetryProvider* telemetry = &telemetryProvider;
