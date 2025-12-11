#include "Xctod.h"
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

int Xctod::calculateCompensatedVoltage(int deciVolts, int deciAmps) {
    float voltage = deciVolts / 10.0;
    float current = deciAmps / 10.0;

    // Apply compensation only if current is significant
    if (abs(current) < BATTERY_CURRENT_THRESHOLD) {
        return deciVolts; // No compensation at rest
    }

    // Calculate total resistance (wire + internal) for all cells in series
    float resistancePerCell = BATTERY_WIRE_RESISTANCE_PER_CELL + BATTERY_INTERNAL_RESISTANCE_PER_CELL;
    float totalResistance = resistancePerCell * BATTERY_CELL_COUNT;

    // Calculate voltage drop (IR drop)
    float voltageDrop = current * totalResistance;

    // Compensated voltage (estimated voltage at rest)
    float compensatedVoltage = voltage + voltageDrop;

    // Return in decivolts, constrained to reasonable limits
    int compensatedDeciVolts = (int)(compensatedVoltage * 10);

    // Constrain to prevent unrealistic values
    int maxDeciVolts = BATTERY_MAX_VOLTAGE + 50; // Allow some margin above max
    int minDeciVolts = BATTERY_MIN_VOLTAGE - 50; // Allow some margin below min

    return constrain(compensatedDeciVolts, minDeciVolts, maxDeciVolts);
}

void Xctod::writeBatteryInfo(String &data) {
    if (!hobbywing.isReady()) {
        data += ",,,"; // battery percentage, voltage, power_kw
        return;
    }

    int batteryDeciVolts = hobbywing.getDeciVoltage();
    int batteryDeciAmps = hobbywing.getDeciCurrent();

    // Calculate compensated voltage to account for IR drop under load
    int compensatedDeciVolts = calculateCompensatedVoltage(batteryDeciVolts, batteryDeciAmps);

    int batteryPercentage = 0;

    // Check if voltage range is valid (min != max)
    if (BATTERY_MIN_VOLTAGE != BATTERY_MAX_VOLTAGE) {
        // Use compensated voltage for percentage calculation
        batteryPercentage = map(
            compensatedDeciVolts,
            BATTERY_MIN_VOLTAGE,
            BATTERY_MAX_VOLTAGE,
            0,
            100
        );
        // Constrain to valid range
        batteryPercentage = constrain(batteryPercentage, 0, 100);
    }

    // Calculate power in KW using current and measured voltage (not compensated)
    float voltage = batteryDeciVolts / 10.0;
    float current = batteryDeciAmps / 10.0;
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