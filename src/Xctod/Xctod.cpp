#include "Xctod.h"
#include "../config.h"
#include "../Throttle/Throttle.h"
#include "../Power/Power.h"
#include "../Temperature/Temperature.h"
#include "../BatteryMonitor/BatteryMonitor.h"
#include "../Logger/Logger.h"

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

extern Throttle throttle;
extern Power power;
extern Temperature motorTemp;
extern BatteryMonitor batteryMonitor;
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

    // Configure CSV header for log file
    logger.setHeader("$XCTOD,battery_percent_cc,battery_percent_voltage,voltage,power_kw,throttle_percent,throttle_raw,power_percent,motor_temp,rpm,esc_current,esc_temp,armed");
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

void Xctod::writeBatteryInfo(String &data) {
    if (!telemetry.hasData()) {
        data += ",,,,"; // battery_percent_cc, battery_percent_voltage, voltage, power_kw
        return;
    }

    // Get SoC from BatteryMonitor (coulomb counting for Hobbywing/Tmotor, voltage for XAG)
    uint8_t batteryPercentageCC = batteryMonitor.getSoC();

    // Get SoC based on voltage only
    uint8_t batteryPercentageVoltage = batteryMonitor.getSoCFromVoltage();

    // Format voltage
    uint16_t millivolts = telemetry.getBatteryVoltageMilliVolts();
    uint16_t volts = millivolts / 1000;
    uint16_t decimals = millivolts % 1000;

    data += String(batteryPercentageCC);
    data += ",";
    data += String(batteryPercentageVoltage);
    data += ",";
    data += String(volts);
    data += ".";
    if (decimals < 10) {
        data += "0";
    }
    data += String(decimals);
    data += ",";

    #if IS_XAG
    // XAG: power not calculable (no current data)
    data += ",";
    #else
    if (!telemetry.hasData()) {
        data += ","; // power_kw
        return;
    }

    // Calculate power in milliwatts: P(mW) = V(mV) * I(mA) / 1000
    uint32_t powerMilliWatts = ((uint32_t)millivolts * telemetry.getBatteryCurrentMilliAmps()) / 1000;
    uint32_t powerKwInt = powerMilliWatts / 1000000;
    uint32_t powerKwDecimal = (powerMilliWatts / 100000) % 10;

    data += String(powerKwInt);
    data += ".";
    data += String(powerKwDecimal);
    data += ",";
    #endif
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
    // Motor temperature: all controllers use telemetry (Hobbywing/Tmotor from CAN+sensor, XAG from sensor)
    if (!telemetry.hasData()) {
        data += ","; // motor temperature not available
    } else {
        int32_t tempCelsius = telemetry.getMotorTempMilliCelsius() / 1000;
        data += String(tempCelsius);
    }
    data += ",";

    #if IS_XAG
    // XAG mode: no RPM or current data available
    data += ",,"; // rpm, current
    #else
    if (!telemetry.hasData()) {
        data += ",,"; // rpm, current
        return;
    }

    data += String(telemetry.getRpm());
    data += ",";
    // Convert current from milliamperes to amperes (integer): I(A) = I(mA) / 1000
    data += String(telemetry.getBatteryCurrentMilliAmps() / 1000);
    data += ",";
    #endif
}

void Xctod::writeEscInfo(String &data) {
    if (!telemetry.hasData()) {
        data += ",";
        return;
    }

    // Format directly from millicelsius using integer division
    int32_t escTempCelsius = telemetry.getEscTempMilliCelsius() / 1000;
    data += String(escTempCelsius);
    data += ",";
}

void Xctod::writeSystemStatus(String &data) {
    data += String(throttle.isArmed() ? "YES" : "NO");
}