#include <Arduino.h>
#include "Xctod.h"
#include "../Throttle/Throttle.h"
#include "../Power/Power.h"
#include "../Canbus/Canbus.h"
#include "../Temperature/Temperature.h"
#include "../config.h"

Xctod::Xctod() {
    lastSerialUpdate = 0;
}

void Xctod::init(unsigned long baudRate) {
    Serial.begin(baudRate);

    Serial.println("$XCTOD,battery_percentage,battery_voltage,power_kw,throttle_percentage,throttle_raw,power_percentage,motor_temp,rpm,current,esc_temp,armed,cruise");
    Serial.println("$XCTOD,,,,,,,,,,,");
}

void Xctod::write() {
    if (millis() - lastSerialUpdate < UPDATE_INTERVAL) {
        return;
    }

    lastSerialUpdate = millis();

    Serial.print("$XCTOD,");

    writeBatteryInfo();
    writeThrottleInfo();
    writeMotorInfo();
    writeEscInfo();
    writeSystemStatus();

    Serial.println("");
}

void Xctod::writeBatteryInfo() {
    if (!hobbywing.isReady()) {
        Serial.print(",,"); // battery percentage and voltage
        return;
    }

    int batteryDeciVolts = hobbywing.getDeciVoltage();
    int batteryPercentage = map(
        batteryDeciVolts,
        BATTERY_MIN_VOLTAGE,
        BATTERY_MAX_VOLTAGE,
        0,
        100
    );

    // Calculate power in KW using current and voltage
    float voltage = batteryDeciVolts / 10.0;
    float current = hobbywing.getDeciCurrent() / 10.0;
    float powerKw = (voltage * current) / 1000.0; // Convert to KW

    Serial.print(batteryPercentage);
    Serial.print(",");
    Serial.print(voltage, 2);
    Serial.print(",");
    Serial.print(powerKw, 1);
    Serial.print(",");
}

void Xctod::writeThrottleInfo() {
    unsigned int throttlePercentage = throttle.getThrottlePercentage();
    unsigned int throttleRaw = throttle.getThrottleRaw();
    unsigned int powerPercentage = power.getPower();

    Serial.print(throttlePercentage);
    Serial.print(",");
    Serial.print(throttleRaw);
    Serial.print(",");
    Serial.print(powerPercentage);
    Serial.print(",");
}

void Xctod::writeMotorInfo() {
    Serial.print(motorTemp.getTemperature(), 0);
    Serial.print(",");

    if (!hobbywing.isReady()) {
        Serial.print(",,"); // temperature, rpm, current
        return;
    }

    Serial.print(hobbywing.getRpm());
    Serial.print(",");
    Serial.print(hobbywing.getDeciCurrent() / 10.0, 2);
    Serial.print(",");
}

void Xctod::writeEscInfo() {
    if (!hobbywing.isReady()) {
        Serial.print(",");
        return;
    }

    Serial.print(hobbywing.getTemperature());
    Serial.print(",");
}

void Xctod::writeSystemStatus() {
    Serial.print(throttle.isArmed() ? "YES" : "NO");
    Serial.print(",");
    Serial.print(throttle.isCruising() ? "ON" : "OFF");
    Serial.print(",");
}