#include "BluetoothBms.h"
#include "../config.h"
#include "../JbdBms/JbdBms.h"

void BluetoothBms::init() {
    jbdBms.init();
}

void BluetoothBms::update() {
    jbdBms.update();
}

bool BluetoothBms::isConnected() const {
    return jbdBms.isConnected();
}

bool BluetoothBms::hasData() const {
    return jbdBms.hasData();
}

bool BluetoothBms::hasCellData() const {
    return jbdBms.hasCellData();
}

uint32_t BluetoothBms::getPackVoltageMilliVolts() const {
    return jbdBms.getPackVoltageMilliVolts();
}

int32_t BluetoothBms::getPackCurrentMilliAmps() const {
    return jbdBms.getPackCurrentMilliAmps();
}

uint8_t BluetoothBms::getSoCPercent() const {
    return jbdBms.getSoCPercent();
}

uint8_t BluetoothBms::getCellCount() const {
    return jbdBms.getCellCount();
}

uint8_t BluetoothBms::getTempCount() const {
    return jbdBms.getNtcCount();
}

int16_t BluetoothBms::getTempCelsius(uint8_t index) const {
    return jbdBms.getNtcTempCelsius(index);
}

uint16_t BluetoothBms::getCellVoltageMilliVolts(uint8_t index) const {
    return jbdBms.getCellVoltageMilliVolts(index);
}

uint16_t BluetoothBms::getCellMinMilliVolts() const {
    return jbdBms.getCellMinMilliVolts();
}

uint16_t BluetoothBms::getCellMaxMilliVolts() const {
    return jbdBms.getCellMaxMilliVolts();
}

uint16_t BluetoothBms::getCellDeltaMilliVolts() const {
    return jbdBms.getCellDeltaMilliVolts();
}
