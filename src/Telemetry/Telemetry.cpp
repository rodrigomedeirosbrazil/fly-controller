#include "Telemetry.h"
#include "../config_controller.h"
#include "../config.h"

void Telemetry::init() {
#if IS_HOBBYWING
    // Hobbywing init done in main.cpp (ESC setup)
    (void)0;
#elif IS_TMOTOR
    // Tmotor init done in main.cpp
    (void)0;
#elif IS_XAG
    // XAG: sensors ready after main.cpp ADC setup
    (void)0;
#endif
}

void Telemetry::update() {
#if IS_HOBBYWING
    hobbywingTelemetry.update();
#elif IS_TMOTOR
    tmotorTelemetry.update();
#elif IS_XAG
    xagTelemetry.update();
#endif
}

bool Telemetry::hasData() const {
#if IS_HOBBYWING
    return hobbywingTelemetry.hasData();
#elif IS_TMOTOR
    return tmotorTelemetry.hasData();
#elif IS_XAG
    return xagTelemetry.hasData();
#else
    return false;
#endif
}

uint16_t Telemetry::getBatteryVoltageMilliVolts() const {
#if IS_HOBBYWING
    return hobbywingTelemetry.getBatteryVoltageMilliVolts();
#elif IS_TMOTOR
    return tmotorTelemetry.getBatteryVoltageMilliVolts();
#elif IS_XAG
    return xagTelemetry.getBatteryVoltageMilliVolts();
#else
    return 0;
#endif
}

uint32_t Telemetry::getBatteryCurrentMilliAmps() const {
#if IS_HOBBYWING
    return hobbywingTelemetry.getBatteryCurrentMilliAmps();
#elif IS_TMOTOR
    return tmotorTelemetry.getBatteryCurrentMilliAmps();
#elif IS_XAG
    return xagTelemetry.getBatteryCurrentMilliAmps();
#else
    return 0;
#endif
}

uint16_t Telemetry::getRpm() const {
#if IS_HOBBYWING
    return hobbywingTelemetry.getRpm();
#elif IS_TMOTOR
    return tmotorTelemetry.getRpm();
#elif IS_XAG
    return xagTelemetry.getRpm();
#else
    return 0;
#endif
}

int32_t Telemetry::getMotorTempMilliCelsius() const {
#if IS_HOBBYWING
    return hobbywingTelemetry.getMotorTempMilliCelsius();
#elif IS_TMOTOR
    return tmotorTelemetry.getMotorTempMilliCelsius();
#elif IS_XAG
    return xagTelemetry.getMotorTempMilliCelsius();
#else
    return 0;
#endif
}

int32_t Telemetry::getEscTempMilliCelsius() const {
#if IS_HOBBYWING
    return hobbywingTelemetry.getEscTempMilliCelsius();
#elif IS_TMOTOR
    return tmotorTelemetry.getEscTempMilliCelsius();
#elif IS_XAG
    return xagTelemetry.getEscTempMilliCelsius();
#else
    return 0;
#endif
}

unsigned long Telemetry::getLastUpdate() const {
#if IS_HOBBYWING
    return hobbywingTelemetry.getLastUpdate();
#elif IS_TMOTOR
    return tmotorTelemetry.getLastUpdate();
#elif IS_XAG
    return xagTelemetry.getLastUpdate();
#else
    return 0;
#endif
}

Telemetry telemetry;
