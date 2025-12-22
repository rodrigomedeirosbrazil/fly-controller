#include "Xctod.h"
#include <cmath>
#include "../Throttle/Throttle.h"
#include "../Power/Power.h"
#include "../Canbus/Canbus.h"
#include "../Temperature/Temperature.h"
#include "../Hobbywing/Hobbywing.h"
#include "../config.h"
#include "../Logger/Logger.h"

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

extern Throttle throttle;
extern Power power;
extern Temperature motorTemp;
extern Hobbywing hobbywing;
extern Logger logger;

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    Serial.println("BLE connected");
  }
  void onDisconnect(BLEServer* pServer) {
    Serial.println("BLE disconnected");
    delay(500);
    pServer->getAdvertising()->start();
  }
};

Xctod::Xctod() {
    lastUpdate = 0;
    batteryCapacityAh = 65.0;
    remainingAh = 65.0;
    lastCoulombTs = 0;
    pServer = nullptr;
    pService = nullptr;
    pCharacteristic = nullptr;
}

void Xctod::init() {
    // Create the BLE Device
    BLEDevice::init("FlyController");

    // Create the BLE Server
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    // Create the BLE Service
    pService = pServer->createService(SERVICE_UUID);

    // Create a BLE Characteristic
    pCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID_TX,
                        BLECharacteristic::PROPERTY_NOTIFY
                      );

    pCharacteristic->addDescriptor(new BLE2902());

    // Start the service
    pService->start();

    // Start advertising
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMaxPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.println("BLE advertising started");
}

void Xctod::write() {
    if (millis() - lastUpdate < UPDATE_INTERVAL) {
        return;
    }

    lastUpdate = millis();

    updateCoulombCount();

    String data = "$XCTOD,";

    writeBatteryInfo(data);
    writeThrottleInfo(data);
    writeMotorInfo(data);
    writeEscInfo(data);
    writeSystemStatus(data);
    data += "\r\n";

    logger.log(data);

    pCharacteristic->setValue(data.c_str());
    pCharacteristic->notify();
}

void Xctod::updateCoulombCount() {
    if (!hobbywing.isReady()) {
        return;
    }

    unsigned long currentTs = millis();

    // Initialize timestamp on first call
    if (lastCoulombTs == 0) {
        lastCoulombTs = currentTs;
        // Recalibrate from voltage on first call if system is ready
        recalibrateFromVoltage();
        return;
    }

    // Check if we should recalibrate from voltage (no load condition)
    float currentA = hobbywing.getDeciCurrent() / 10.0;
    if (fabs(currentA) < ZERO_CURRENT_THRESHOLD_A) {
        recalibrateFromVoltage();
        // Still update timestamp to avoid accumulation of time
        lastCoulombTs = currentTs;
        return;
    }

    // Calculate time delta in hours
    unsigned long deltaMs = currentTs - lastCoulombTs;
    float deltaHours = deltaMs / 3600000.0;  // Convert milliseconds to hours

    // Calculate Ah consumed: ΔAh = I * Δt
    float deltaAh = currentA * deltaHours;

    // Subtract from remaining capacity (discharging = positive current)
    remainingAh -= deltaAh;

    // Constrain to valid range: 0.0 ≤ remainingAh ≤ batteryCapacityAh
    remainingAh = constrain(remainingAh, 0.0, batteryCapacityAh);

    // Update timestamp
    lastCoulombTs = currentTs;
}

void Xctod::recalibrateFromVoltage() {
    if (!hobbywing.isReady()) {
        return;
    }

    // Check if voltage range is valid (min != max)
    if (BATTERY_MIN_VOLTAGE == BATTERY_MAX_VOLTAGE) {
        return;
    }

    int batteryDeciVolts = hobbywing.getDeciVoltage();

    // Map voltage to percentage (0-100)
    int voltagePercentage = map(
        batteryDeciVolts,
        BATTERY_MIN_VOLTAGE,
        BATTERY_MAX_VOLTAGE,
        0,
        100
    );
    voltagePercentage = constrain(voltagePercentage, 0, 100);

    // Calculate remainingAh from voltage percentage
    remainingAh = (voltagePercentage / 100.0) * batteryCapacityAh;

    // Constrain to valid range
    remainingAh = constrain(remainingAh, 0.0, batteryCapacityAh);
}

void Xctod::writeBatteryInfo(String &data) {
    if (!hobbywing.isReady()) {
        data += ",,,"; // battery percentage, voltage, power_kw
        return;
    }

    // Calculate battery percentage from Coulomb Counting
    int batteryPercentage = (remainingAh / batteryCapacityAh) * 100.0;
    batteryPercentage = constrain(batteryPercentage, 0, 100);

    // Get voltage for display and power calculation (not for SoC)
    int batteryDeciVolts = hobbywing.getDeciVoltage();
    float voltage = batteryDeciVolts / 10.0;

    // Calculate power in KW using current and voltage
    float current = hobbywing.getDeciCurrent() / 10.0;
    float powerKw = (voltage * current) / 1000.0; // Convert to KW

    data += String(batteryPercentage);
    data += ",";
    data += String(voltage, 2);
    data += ",";
    data += String(powerKw, 1);
    data += ",";
}

void Xctod::writeThrottleInfo(String &data) {
    unsigned int throttlePercentage = throttle.getThrottlePercentage();
    unsigned int throttleRaw = throttle.getThrottleRaw();
    unsigned int powerPercentage = power.getPower();

    data += String(throttlePercentage);
    data += ",";
    data += String(throttleRaw);
    data += ",";
    data += String(powerPercentage);
    data += ",";
}

void Xctod::writeMotorInfo(String &data) {
    data += String(motorTemp.getTemperature(), 0);
    data += ",";

    if (!hobbywing.isReady()) {
        data += ",,"; // rpm, current
        return;
    }

    data += String(hobbywing.getRpm());
    data += ",";
    data += String(hobbywing.getDeciCurrent() / 10.0, 2);
    data += ",";
}

void Xctod::writeEscInfo(String &data) {
    if (!hobbywing.isReady()) {
        data += ",";
        return;
    }

    data += String(hobbywing.getTemperature());
    data += ",";
}

void Xctod::writeSystemStatus(String &data) {
    data += String(throttle.isArmed() ? "YES" : "NO");
}