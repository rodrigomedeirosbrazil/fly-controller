#include <Arduino.h>
#include "SerialScreen.h"

SerialScreen::SerialScreen() {
    this->lastSerialUpdate = 0;
}

void SerialScreen::init(unsigned long baudRate) {
    Serial.begin(baudRate);
    Serial.println("Serial monitor initialized.");
    Serial.println("FLY CONTROLLER SERIAL MONITORING");
    Serial.println("--------------------------------");
}

void SerialScreen::clearScreen() {
    // ANSI escape code to clear screen and position cursor at top-left
    Serial.write("\033[2J\033[H");
}

void SerialScreen::write() {
    // Update every 1 second to avoid flooding the serial monitor
    if (millis() - lastSerialUpdate < 1000) {
        return;
    }

    lastSerialUpdate = millis();

    // Clear the screen before displaying new information
    clearScreen();

    // Check if throttle needs calibration
    if (!throttle.isCalibrated()) {
        writeCalibrationInfo();
        return;
    }

    // Create a compact display with all information
    Serial.println("========= FLY CONTROLLER STATUS =========");

    writeBatteryInfo();
    writeThrottleInfo();
    writeMotorInfo();
    writeEscInfo();
    writeSystemStatus();

    Serial.println("========================================");
}

void SerialScreen::writeCalibrationInfo() {
    // Get the current filtered throttle value and calibrating step
    int pinValueFiltered = throttle.getPinValueFiltered();
    unsigned int calibratingStep = throttle.getCalibratingStep();

    Serial.println("======== THROTTLE CALIBRATION MODE ========");

    // Check which calibration step we're on and provide appropriate instructions
    if (calibratingStep == 0) {
        Serial.println("STEP 1: Set throttle to 100% (max position)");
        Serial.println("        Hold for 3 seconds to calibrate");
    } else {
        Serial.println("STEP 2: Set throttle to 0% (min position)");
        Serial.println("        Hold for 3 seconds to calibrate");
    }

    Serial.print("Raw value: ");
    Serial.println(pinValueFiltered);

    // Visual progress bar for raw value (scaled to typical analog range 0-1023)
    Serial.print("[");
    const unsigned int segmentCount = 30;
    // Use unsigned long for calculation to avoid overflow with large pinValueFiltered values
    unsigned long scaledValue = (static_cast<unsigned long>(pinValueFiltered) * segmentCount) / 1023;

    for (unsigned int i = 0; i < segmentCount; i++) {
        if (i < scaledValue) {
            Serial.print("=");
        } else {
            Serial.print(" ");
        }
    }
    Serial.println("]");
    Serial.println("");

    Serial.println("========================================");
}

void SerialScreen::writeBatteryInfo() {
    if (!canbus.isReady()) {
        Serial.println("BAT: ---% | ---.-V");
        return;
    }

    int batteryDeciVolts = canbus.getDeciVoltage();
    int batteryPercentage = map(
        batteryDeciVolts,
        BATTERY_MIN_VOLTAGE,
        BATTERY_MAX_VOLTAGE,
        0,
        100
    );

    Serial.print("BAT: ");
    Serial.print(batteryPercentage);

    Serial.print("% | ");
    Serial.print(batteryDeciVolts / 10.0, 2);
    Serial.println("V");
}

void SerialScreen::writeThrottleInfo() {
    unsigned int throttlePercentage = throttle.getThrottlePercentage();
    unsigned int powerPercentage = power.getPower();

    Serial.print("THROTTLE: ");
    Serial.print(throttlePercentage);
    Serial.print("%");

    Serial.print(" POWER: ");
    Serial.print(powerPercentage);
    Serial.println("%");
}

void SerialScreen::writeMotorInfo() {
    Serial.print("MOTOR: T: ");
    Serial.print(motorTemp.getTemperature(), 0);
    Serial.print("C | ");

    if (!canbus.isReady()) {
        Serial.println("RPM: ---- | A: ---");
        return;
    }

    Serial.print("RPM: ");
    Serial.print(canbus.getRpm());
    Serial.print(" | A: ");
    Serial.print(canbus.getDeciCurrent() / 10.0, 2);
    Serial.println("A");
}

void SerialScreen::writeEscInfo() {
    Serial.print("ESC: T: ");

    if (!canbus.isReady()) {
        Serial.println("--C");
        return;
    }

    Serial.print(canbus.getTemperature());
    Serial.println("C");
}

void SerialScreen::writeSystemStatus() {
    Serial.print("STATUS: Armed:");
    Serial.print(throttle.isArmed() ? "YES" : "NO");
    Serial.print(" | Cruise:");
    Serial.println(throttle.isCruising() ? "ON" : "OFF");
}
