#include "Xctod.h"
#include "../Telemetry/TelemetryProvider.h"
#include "../Telemetry/TelemetryData.h"
#include "../Throttle/Throttle.h"
#include "../Power/Power.h"
#include "../Temperature/Temperature.h"
#include "../config.h"
#include "../Logger/Logger.h"

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

extern Throttle throttle;
extern Power power;
extern Temperature motorTemp;
extern TelemetryProvider* telemetry;
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
    batteryCapacityMilliAh = 65000;  // 65.0 Ah = 65000 mAh
    remainingMilliAh = 65000;
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
    Serial.println("$XCTOD,battery_percent,voltage,power_kw,throttle_percent,throttle_raw,power_percent,motor_temp,rpm,esc_current,esc_temp,armed");
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
    if (!telemetry || !telemetry->getData()) {
        return;
    }

    TelemetryData* data = telemetry->getData();

    #if IS_XAG
    // XAG mode: no current data available, skip Coulomb counting
    return;
    #else
    if (!data->hasTelemetry) {
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
    if (data->batteryCurrentMilliAmps == 0) {
        recalibrateFromVoltage();
        // Still update timestamp to avoid accumulation of time
        lastCoulombTs = currentTs;
        return;
    }

    // Calculate time delta in milliseconds
    unsigned long deltaMs = currentTs - lastCoulombTs;

    // Calculate mAh consumed using integer arithmetic: ΔmAh = (I(mA) * Δt(ms)) / 3600000
    // This avoids float conversion: current is in milliamperes, deltaMs in milliseconds
    uint32_t deltaMilliAh = (data->batteryCurrentMilliAmps * deltaMs) / 3600000;

    // Subtract from remaining capacity (discharging = positive current)
    if (deltaMilliAh <= remainingMilliAh) {
        remainingMilliAh -= deltaMilliAh;
    } else {
        remainingMilliAh = 0;  // Prevent underflow
    }

    // Constrain to valid range: 0 ≤ remainingMilliAh ≤ batteryCapacityMilliAh
    if (remainingMilliAh > batteryCapacityMilliAh) {
        remainingMilliAh = batteryCapacityMilliAh;
    }

    // Update timestamp
    lastCoulombTs = currentTs;
    #endif
}

void Xctod::recalibrateFromVoltage() {
    if (!telemetry || !telemetry->getData()) {
        return;
    }

    TelemetryData* data = telemetry->getData();

    #if IS_XAG
    // XAG mode: no voltage data available, skip recalibration
    return;
    #else
    if (!data->hasTelemetry) {
        return;
    }

    // Check if voltage range is valid (min != max)
    if (BATTERY_MIN_VOLTAGE == BATTERY_MAX_VOLTAGE) {
        return;
    }

    // Use millivolts directly for comparison
    uint16_t batteryMilliVolts = data->batteryVoltageMilliVolts;

    // Map voltage to percentage (0-100)
    int voltagePercentage = map(
        batteryMilliVolts,
        BATTERY_MIN_VOLTAGE,
        BATTERY_MAX_VOLTAGE,
        0,
        100
    );
    voltagePercentage = constrain(voltagePercentage, 0, 100);

    // Calculate remainingMilliAh from voltage percentage using integer arithmetic
    remainingMilliAh = ((uint32_t)voltagePercentage * batteryCapacityMilliAh) / 100;

    // Constrain to valid range: 0 ≤ remainingMilliAh ≤ batteryCapacityMilliAh
    if (remainingMilliAh > batteryCapacityMilliAh) {
        remainingMilliAh = batteryCapacityMilliAh;
    }
    #endif
}

void Xctod::writeBatteryInfo(String &data) {
    if (!telemetry || !telemetry->getData()) {
        data += ",,,"; // battery percentage, voltage, power_kw
        return;
    }

    TelemetryData* telemetryData = telemetry->getData();

    #if IS_XAG
    // XAG mode: read battery voltage from telemetry
    // Calculate battery percentage from voltage (not reliable for power control)
    int voltagePercentage = map(
        telemetryData->batteryVoltageMilliVolts,
        BATTERY_MIN_VOLTAGE,
        BATTERY_MAX_VOLTAGE,
        0,
        100
    );
    voltagePercentage = constrain(voltagePercentage, 0, 100);

    // Power is not calculated in XAG mode (no current data)
    data += String(voltagePercentage);
    data += ",";
    // Format directly from millivolts using integer operations only
    uint16_t millivolts = telemetryData->batteryVoltageMilliVolts;
    uint16_t volts = millivolts / 1000;
    uint16_t decimals = millivolts % 1000;
    data += String(volts);
    data += ".";
    if (decimals < 10) {
        data += "00";
    } else if (decimals < 100) {
        data += "0";
    }
    data += String(decimals);
    data += ",";
    data += ","; // power_kw (not available)
    #else
    if (!telemetryData->hasTelemetry) {
        data += ",,,"; // battery percentage, voltage, power_kw
        return;
    }

    // Calculate battery percentage from Coulomb Counting using integer arithmetic
    int batteryPercentage = (remainingMilliAh * 100) / batteryCapacityMilliAh;
    batteryPercentage = constrain(batteryPercentage, 0, 100);

    // Format voltage directly from millivolts using integer operations only
    uint16_t millivolts = telemetryData->batteryVoltageMilliVolts;
    uint16_t volts = millivolts / 1000;
    uint16_t decimals = millivolts % 1000;

    // Calculate power in milliwatts using integer arithmetic: P(mW) = V(mV) * I(mA) / 1000
    uint32_t powerMilliWatts = ((uint32_t)millivolts * telemetryData->batteryCurrentMilliAmps) / 1000;
    // Convert to kW for display: divide by 1,000,000
    uint32_t powerKwInt = powerMilliWatts / 1000000;
    uint32_t powerKwDecimal = (powerMilliWatts / 100000) % 10; // First decimal place

    data += String(batteryPercentage);
    data += ",";
    data += String(volts);
    data += ".";
    if (decimals < 10) {
        data += "00";
    } else if (decimals < 100) {
        data += "0";
    }
    data += String(decimals);
    data += ",";
    // Format power in kW with 1 decimal place
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
    #if IS_TMOTOR
    // Tmotor: use motor temperature from telemetry (received via CAN)
    if (!telemetry || !telemetry->getData()) {
        data += ","; // motor temperature not available
    } else {
        TelemetryData* telemetryData = telemetry->getData();
        // Format directly from millicelsius using integer division
        int32_t tempCelsius = telemetryData->motorTemperatureMilliCelsius / 1000;
        data += String(tempCelsius);
    }
    data += ",";
    #else
    // Other controllers: motor temperature is read from sensor (ADC)
    data += String(motorTemp.getTemperature(), 0);
    data += ",";
    #endif

    if (!telemetry || !telemetry->getData()) {
        data += ",,"; // rpm, current
        return;
    }

    TelemetryData* telemetryData = telemetry->getData();

    #if IS_XAG
    // XAG mode: no RPM or current data available
    data += ",,"; // rpm, current
    #else
    if (!telemetryData->hasTelemetry) {
        data += ",,"; // rpm, current
        return;
    }

    data += String(telemetryData->rpm);
    data += ",";
    // Convert current from milliamperes to amperes (integer): I(A) = I(mA) / 1000
    data += String(telemetryData->batteryCurrentMilliAmps / 1000);
    data += ",";
    #endif
}

void Xctod::writeEscInfo(String &data) {
    if (!telemetry || !telemetry->getData()) {
        data += ",";
        return;
    }

    TelemetryData* telemetryData = telemetry->getData();

    // Format directly from millicelsius using integer division
    int32_t escTempCelsius = telemetryData->escTemperatureMilliCelsius / 1000;
    data += String(escTempCelsius);
    data += ",";
}

void Xctod::writeSystemStatus(String &data) {
    data += String(throttle.isArmed() ? "YES" : "NO");
}