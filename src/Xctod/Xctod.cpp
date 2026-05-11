#include "Xctod.h"
#include "../config.h"
#include "../BoardConfig.h"
#include "../Telemetry/TelemetryAvailability.h"
#include "../Throttle/Throttle.h"
#include "../Power/Power.h"
#include "../Temperature/Temperature.h"
#include "../BatteryMonitor/BatteryMonitor.h"
#include "../BluetoothBms/BluetoothBms.h"
#include <cstdarg>
#include <cstdio>

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

extern Throttle throttle;
extern Power power;
extern Temperature motorTemp;
extern BatteryMonitor batteryMonitor;

namespace {
bool appendToBuffer(char* data, size_t size, size_t& used, const char* format, ...) {
    if (used >= size) {
        return false;
    }

    va_list args;
    va_start(args, format);
    const int written = vsnprintf(data + used, size - used, format, args);
    va_end(args);

    if (written < 0) {
        return false;
    }

    const size_t charsWritten = static_cast<size_t>(written);
    if (charsWritten >= (size - used)) {
        used = size - 1;
        data[used] = '\0';
        return false;
    }

    used += charsWritten;
    return true;
}
} // namespace

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    Serial.println("BLE connected");
  }
  void onDisconnect(BLEServer* pServer) {
    Serial.println("BLE disconnected");
    pServer->getAdvertising()->start();
  }
};

Xctod::Xctod() {
    lastUpdate = 0;
    advertisingEnabled = false;
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
    advertisingEnabled = true;

    Serial.println("BLE advertising started");
}

void Xctod::write() {
    if (millis() - lastUpdate < UPDATE_INTERVAL) {
        return;
    }

    lastUpdate = millis();

    char data[TELEMETRY_BUFFER_SIZE] = {0};
    size_t used = 0;

    appendToBuffer(data, sizeof(data), used, "$XCTOD,");
    writeBatteryInfo(data, sizeof(data), used);
    writeThrottleInfo(data, sizeof(data), used);
    writeMotorInfo(data, sizeof(data), used);
    writeEscInfo(data, sizeof(data), used);
    writeSystemStatus(data, sizeof(data), used);
    writeBmsInfo(data, sizeof(data), used);
    writeMotorTempNtc(data, sizeof(data), used);
    appendToBuffer(data, sizeof(data), used, "\r\n");

    pCharacteristic->setValue(reinterpret_cast<uint8_t*>(data), used);
    pCharacteristic->notify();
}

void Xctod::setAdvertisingEnabled(bool enabled) {
    if (enabled == advertisingEnabled) {
        return;
    }

    if (enabled) {
        BLEDevice::startAdvertising();
    } else {
        BLEDevice::stopAdvertising();
    }

    advertisingEnabled = enabled;
}

bool Xctod::isAdvertisingEnabled() const {
    return advertisingEnabled;
}

void Xctod::writeBatteryInfo(char* data, size_t size, size_t& used) {
    if (!telemetry.hasData()) {
        appendToBuffer(data, size, used, ",,,,"); // battery_percent_cc, battery_percent_voltage, voltage, power_kw
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

    appendToBuffer(data, size, used, "%u,%u,%u.", batteryPercentageCC, batteryPercentageVoltage, volts);
    if (decimals < 10) {
        appendToBuffer(data, size, used, "0");
    }
    appendToBuffer(data, size, used, "%u,", decimals);

    if (isPowerKwAvailable()) {
        uint32_t powerMilliWatts = ((uint32_t)millivolts * telemetry.getBatteryCurrentMilliAmps()) / 1000;
        uint32_t powerKwInt = powerMilliWatts / 1000000;
        uint32_t powerKwDecimal = (powerMilliWatts / 100000) % 10;
        appendToBuffer(data, size, used, "%lu.%lu", (unsigned long)powerKwInt, (unsigned long)powerKwDecimal);
    }
    appendToBuffer(data, size, used, ",");
}

void Xctod::writeThrottleInfo(char* data, size_t size, size_t& used) {
    unsigned int throttlePercentage = throttle.getThrottlePercentage();
    unsigned int throttleRaw = throttle.getThrottleRaw();
    unsigned int powerPercentage = power.getPower();

    appendToBuffer(data, size, used, "%u,%u,%u,", throttlePercentage, throttleRaw, powerPercentage);
}

void Xctod::writeMotorInfo(char* data, size_t size, size_t& used) {
    // Motor temperature: all controllers use telemetry (Hobbywing/Tmotor from CAN+sensor, XAG from sensor)
    if (!telemetry.hasData()) {
        appendToBuffer(data, size, used, ","); // motor temperature not available
    } else {
        int32_t tempCelsius = telemetry.getMotorTempMilliCelsius() / 1000;
        appendToBuffer(data, size, used, "%ld", (long)tempCelsius);
    }
    appendToBuffer(data, size, used, ",");

    if (isCurrentAvailable()) {
        appendToBuffer(
            data,
            size,
            used,
            "%lu,%lu",
            (unsigned long)telemetry.getRpm(),
            (unsigned long)(telemetry.getBatteryCurrentMilliAmps() / 1000)
        );
    } else {
        appendToBuffer(data, size, used, ",");
    }
    appendToBuffer(data, size, used, ",");
}

void Xctod::writeEscInfo(char* data, size_t size, size_t& used) {
    if (!telemetry.hasData()) {
        appendToBuffer(data, size, used, ",");
        return;
    }

    // Format directly from millicelsius using integer division
    int32_t escTempCelsius = telemetry.getEscTempMilliCelsius() / 1000;
    appendToBuffer(data, size, used, "%ld,", (long)escTempCelsius);
}

void Xctod::writeSystemStatus(char* data, size_t size, size_t& used) {
    appendToBuffer(data, size, used, "%s", throttle.isArmed() ? "YES" : "NO");
}

void Xctod::writeBmsInfo(char* data, size_t size, size_t& used) {
    // Use the BluetoothBms facade so this works with both JBD and Daly BMS types.
    if (bluetoothBms.hasData() && bluetoothBms.getTempCount() > 0) {
        int16_t maxTemp = bluetoothBms.getTempCelsius(0);
        for (uint8_t i = 1; i < bluetoothBms.getTempCount(); i++) {
            int16_t t = bluetoothBms.getTempCelsius(i);
            if (t > maxTemp) maxTemp = t;
        }
        appendToBuffer(data, size, used, ",%d", maxTemp);
    } else {
        appendToBuffer(data, size, used, ",");
    }
    if (bluetoothBms.hasCellData()) {
        appendToBuffer(
            data,
            size,
            used,
            ",%u,%u",
            bluetoothBms.getCellMinMilliVolts(),
            bluetoothBms.getCellMaxMilliVolts()
        );
    } else {
        appendToBuffer(data, size, used, ",,");
    }
}

void Xctod::writeMotorTempNtc(char* data, size_t size, size_t& used) {
    int32_t ntcCelsius = telemetry.getMotorTempNtcMilliCelsius() / 1000;
    appendToBuffer(data, size, used, ",%ld", (long)ntcCelsius);
}
