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
    if (data->batteryCurrent == 0) {
        recalibrateFromVoltage();
        // Still update timestamp to avoid accumulation of time
        lastCoulombTs = currentTs;
        return;
    }

    // Calculate time delta in hours
    unsigned long deltaMs = currentTs - lastCoulombTs;
    float deltaHours = deltaMs / 3600000.0;  // Convert milliseconds to hours

    // Calculate Ah consumed: ΔAh = I * Δt
    float currentAmps = (float)data->batteryCurrent;
    float deltaAh = currentAmps * deltaHours;

    // Subtract from remaining capacity (discharging = positive current)
    remainingAh -= deltaAh;

    // Constrain to valid range: 0.0 ≤ remainingAh ≤ batteryCapacityAh
    remainingAh = constrain(remainingAh, 0.0, batteryCapacityAh);

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

    // Calculate remainingAh from voltage percentage
    remainingAh = (voltagePercentage / 100.0) * batteryCapacityAh;

    // Constrain to valid range
    remainingAh = constrain(remainingAh, 0.0, batteryCapacityAh);
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
    float voltage = milliVoltsToVolts(telemetryData->batteryVoltageMilliVolts);

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

    // Calculate battery percentage from Coulomb Counting
    int batteryPercentage = (remainingAh / batteryCapacityAh) * 100.0;
    batteryPercentage = constrain(batteryPercentage, 0, 100);

    // Format voltage directly from millivolts using integer operations only
    uint16_t millivolts = telemetryData->batteryVoltageMilliVolts;
    uint16_t volts = millivolts / 1000;
    uint16_t decimals = millivolts % 1000;
    float voltage = millivolts / 1000.0f; // Still need float for power calculation

    // Calculate power in KW using current and voltage
    float current = (float)telemetryData->batteryCurrent; // Current is already in amperes
    float powerKw = (voltage * current) / 1000.0; // Convert to KW

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
    data += String(powerKw, 1);
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
        float motorTempCelsius = milliCelsiusToCelsius(telemetryData->motorTemperatureMilliCelsius);
        data += String(motorTempCelsius, 0);
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
    // Current is already an integer in amperes, format directly without float conversion
    data += String(telemetryData->batteryCurrent);
    data += ",";
    #endif
}

void Xctod::writeEscInfo(String &data) {
    if (!telemetry || !telemetry->getData()) {
        data += ",";
        return;
    }

    TelemetryData* telemetryData = telemetry->getData();

    // Convert millicelsius to celsius for display
    float escTempCelsius = milliCelsiusToCelsius(telemetryData->escTemperatureMilliCelsius);
    data += String(escTempCelsius, 0);
    data += ",";
}

void Xctod::writeSystemStatus(String &data) {
    data += String(throttle.isArmed() ? "YES" : "NO");
}