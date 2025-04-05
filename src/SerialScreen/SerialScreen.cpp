#include <Arduino.h>
#include "SerialScreen.h"

SerialScreen::SerialScreen(
    Throttle *throttle,
    Canbus *canbus,
    Temperature *motorTemp
) {
    this->throttle = throttle;
    this->canbus = canbus;
    this->motorTemp = motorTemp;
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
    if (!this->throttle->isCalibrated()) {
        writeCalibrationInfo();
        return;
    }

    Serial.println("--------------------------------");
    Serial.println("FLY CONTROLLER STATUS");
    Serial.println("--------------------------------");

    writeBatteryInfo();
    writeThrottleInfo();
    writeMotorInfo();
    writeEscInfo();
    writeSystemStatus();

    Serial.println("--------------------------------");
}

void SerialScreen::writeCalibrationInfo() {
    // Get the current filtered throttle value and calibrating step
    int pinValueFiltered = this->throttle->getPinValueFiltered();
    unsigned int calibratingStep = this->throttle->getCalibratingStep();

    Serial.println("THROTTLE CALIBRATION MODE");
    Serial.println("\nCALIBRATION INSTRUCTIONS:");

    // Check which calibration step we're on and provide appropriate instructions
    if (calibratingStep == 0) {
        Serial.println("  Step 1: Set throttle to 100% (maximum position)");
        Serial.println("     Hold for 3 seconds to calibrate maximum value");
    } else {
        Serial.println("  Step 2: Set throttle to 0% (minimum position)");
        Serial.println("     Hold for 3 seconds to calibrate minimum value");
    }

    Serial.println("\nCURRENT THROTTLE READINGS:");
    Serial.print("  Raw value: ");
    Serial.println(pinValueFiltered);
    Serial.println("--------------------------------");
}

void SerialScreen::writeBatteryInfo() {
    Serial.println("BATTERY INFO:");

    if (!canbus->isReady()) {
        Serial.println("  Battery: ---%");
        Serial.println("  Voltage: --.-V");
        return;
    }

    int batteryMilliVolts = canbus->getMiliVoltage();
    int batteryPercentage = map(
        batteryMilliVolts,
        BATTERY_MIN_VOLTAGE,
        BATTERY_MAX_VOLTAGE,
        0,
        100
    );

    Serial.print("  Battery: ");
    Serial.print(batteryPercentage);
    Serial.println("%");

    Serial.print("  Voltage: ");
    Serial.print(batteryMilliVolts / 10.0, 1);  // Convert to volts with 1 decimal place
    Serial.println("V");
}

void SerialScreen::writeThrottleInfo() {
    Serial.println("THROTTLE INFO:");

    unsigned int throttlePercentage = this->throttle->getThrottlePercentage();

    Serial.print("  Throttle: ");
    Serial.print(throttlePercentage);
    Serial.println("%");
}

void SerialScreen::writeMotorInfo() {
    Serial.println("MOTOR INFO:");

    Serial.print("  Temperature: ");
    Serial.print(motorTemp->getTemperature(), 0);  // 0 decimal places
    Serial.println("C");

    if (!canbus->isReady()) {
        Serial.println("  RPM: ----");
        Serial.println("  Current: ---A");
        return;
    }

    Serial.print("  RPM: ");
    Serial.println(canbus->getRpm());

    Serial.print("  Current: ");
    Serial.print(canbus->getMiliCurrent() / 10.0, 0);  // Convert to amps with 0 decimal places
    Serial.println("A");
}

void SerialScreen::writeEscInfo() {
    Serial.println("ESC INFO:");

    if (!canbus->isReady()) {
        Serial.println("  Temperature: --C");
        return;
    }

    Serial.print("  Temperature: ");
    Serial.print(canbus->getTemperature());
    Serial.println("C");
}

void SerialScreen::writeSystemStatus() {
    Serial.println("SYSTEM STATUS:");

    Serial.print("  Armed: ");
    Serial.println(this->throttle->isArmed() ? "YES" : "NO");

    Serial.print("  Cruise Control: ");
    Serial.println(this->throttle->isCruising() ? "ACTIVE" : "INACTIVE");
}
