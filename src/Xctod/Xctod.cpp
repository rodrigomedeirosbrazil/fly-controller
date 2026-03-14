#include "Xctod.h"
#include "../config.h"
#include "../BoardConfig.h"
#include "../Throttle/Throttle.h"
#include "../Power/Power.h"
#include "../Temperature/Temperature.h"
#include "../BatteryMonitor/BatteryMonitor.h"
#include "../Logger/Logger.h"
#include "../JbdBms/JbdBms.h"

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

    // XCTOD CSV fields (name and index):
    //  0: $XCTOD (prefix)
    //  1: battery_percent_cc
    //  2: battery_percent_voltage
    //  3: voltage
    //  4: power_kw
    //  5: throttle_percent
    //  6: throttle_raw
    //  7: power_percent
    //  8: motor_temp
    //  9: rpm
    // 10: esc_current
    // 11: esc_temp
    // 12: armed
    // 13: battery_temp_max (JBD), 14: cell_voltage_min_mv, 15: cell_voltage_max_mv
    logger.setHeader("$XCTOD,battery_percent_cc,battery_percent_voltage,voltage,power_kw,throttle_percent,throttle_raw,power_percent,motor_temp,rpm,esc_current,esc_temp,armed,battery_temp_max,cell_voltage_min_mv,cell_voltage_max_mv");
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
    writeBmsInfo(data);
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

    if (getBoardConfig().hasCurrentSensor && telemetry.hasData()) {
        uint32_t powerMilliWatts = ((uint32_t)millivolts * telemetry.getBatteryCurrentMilliAmps()) / 1000;
        uint32_t powerKwInt = powerMilliWatts / 1000000;
        uint32_t powerKwDecimal = (powerMilliWatts / 100000) % 10;
        data += String(powerKwInt);
        data += ".";
        data += String(powerKwDecimal);
    }
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
    // Motor temperature: all controllers use telemetry (Hobbywing/Tmotor from CAN+sensor, XAG from sensor)
    if (!telemetry.hasData()) {
        data += ","; // motor temperature not available
    } else {
        int32_t tempCelsius = telemetry.getMotorTempMilliCelsius() / 1000;
        data += String(tempCelsius);
    }
    data += ",";

    if (getBoardConfig().hasCurrentSensor && telemetry.hasData()) {
        data += String(telemetry.getRpm());
        data += ",";
        data += String(telemetry.getBatteryCurrentMilliAmps() / 1000);
    } else {
        data += ",";
    }
    data += ",";
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

void Xctod::writeBmsInfo(String &data) {
    if (jbdBms.hasData() && jbdBms.getNtcCount() > 0) {
        int16_t maxTemp = jbdBms.getNtcTempCelsius(0);
        for (uint8_t i = 1; i < jbdBms.getNtcCount(); i++) {
            int16_t t = jbdBms.getNtcTempCelsius(i);
            if (t > maxTemp) maxTemp = t;
        }
        data += ",";
        data += String(maxTemp);
    } else {
        data += ",";
    }
    if (jbdBms.hasCellData()) {
        data += ",";
        data += String(jbdBms.getCellMinMilliVolts());
        data += ",";
        data += String(jbdBms.getCellMaxMilliVolts());
    } else {
        data += ",,";
    }
}