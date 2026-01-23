#include "config.h"
#include "Telemetry/TelemetryProvider.h"
#include "Power/Power.h"
#include "Xctod/Xctod.h"

// Select telemetry provider based on build configuration
#ifdef XAG
    #include "Telemetry/XagProvider.h"
    static TelemetryProvider telemetryProvider = createXagProvider();
#elif defined(T_MOTOR)
    #include "Telemetry/TmotorProvider.h"
    static TelemetryProvider telemetryProvider = createTmotorProvider();
#else
    #include "Telemetry/HobbywingProvider.h"
    static TelemetryProvider telemetryProvider = createHobbywingProvider();
#endif

Buzzer buzzer(BUZZER_PIN);
Servo esc;
Throttle throttle;
#ifndef XAG
Canbus canbus;
#ifdef T_MOTOR
Tmotor tmotor;
#else
Hobbywing hobbywing;
#endif
twai_message_t canMsg;
#endif
Button button(BUTTON_PIN);
Temperature motorTemp(MOTOR_TEMPERATURE_PIN);
#ifdef XAG
Temperature escTemp(ESC_TEMPERATURE_PIN);
#endif

Power power;
Xctod xctod;

// Define telemetry pointer (declared above after includes)
TelemetryProvider* telemetry = &telemetryProvider;
