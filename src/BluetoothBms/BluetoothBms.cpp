#include "BluetoothBms.h"
#include "../config.h"
#include "../DalyBms/DalyBms.h"
#include "../JbdBms/JbdBms.h"
#include "../Settings/Settings.h"

void BluetoothBms::init() {
    jbdBms.init();
    dalyBms.init();
}

void BluetoothBms::update() {
    const uint8_t activeType = getActiveType();
    const String mac = settings.getBmsMac();

    jbdBms.setMacAddress(mac);
    jbdBms.setEnabled(activeType == BmsTypeJbd);

    dalyBms.setMacAddress(mac);
    dalyBms.setEnabled(activeType == BmsTypeDaly);

    if (activeType == BmsTypeJbd) {
        jbdBms.update();
    } else if (activeType == BmsTypeDaly) {
        dalyBms.update();
    }
}

bool BluetoothBms::isConnected() const {
    const uint8_t activeType = getActiveType();
    if (activeType == BmsTypeJbd) {
        return jbdBms.isConnected();
    }
    if (activeType == BmsTypeDaly) {
        return dalyBms.isConnected();
    }
    return false;
}

bool BluetoothBms::hasData() const {
    const uint8_t activeType = getActiveType();
    if (activeType == BmsTypeJbd) {
        return jbdBms.hasData();
    }
    if (activeType == BmsTypeDaly) {
        return dalyBms.hasData();
    }
    return false;
}

bool BluetoothBms::hasCellData() const {
    const uint8_t activeType = getActiveType();
    if (activeType == BmsTypeJbd) {
        return jbdBms.hasCellData();
    }
    if (activeType == BmsTypeDaly) {
        return dalyBms.hasCellData();
    }
    return false;
}

uint32_t BluetoothBms::getPackVoltageMilliVolts() const {
    const uint8_t activeType = getActiveType();
    if (activeType == BmsTypeJbd) {
        return jbdBms.getPackVoltageMilliVolts();
    }
    if (activeType == BmsTypeDaly) {
        return dalyBms.getPackVoltageMilliVolts();
    }
    return 0;
}

int32_t BluetoothBms::getPackCurrentMilliAmps() const {
    const uint8_t activeType = getActiveType();
    if (activeType == BmsTypeJbd) {
        return jbdBms.getPackCurrentMilliAmps();
    }
    if (activeType == BmsTypeDaly) {
        return dalyBms.getPackCurrentMilliAmps();
    }
    return 0;
}

uint8_t BluetoothBms::getSoCPercent() const {
    const uint8_t activeType = getActiveType();
    if (activeType == BmsTypeJbd) {
        return jbdBms.getSoCPercent();
    }
    if (activeType == BmsTypeDaly) {
        return dalyBms.getSoCPercent();
    }
    return 0;
}

uint8_t BluetoothBms::getCellCount() const {
    const uint8_t activeType = getActiveType();
    if (activeType == BmsTypeJbd) {
        return jbdBms.getCellCount();
    }
    if (activeType == BmsTypeDaly) {
        return dalyBms.getCellCount();
    }
    return 0;
}

uint8_t BluetoothBms::getTempCount() const {
    const uint8_t activeType = getActiveType();
    if (activeType == BmsTypeJbd) {
        return jbdBms.getNtcCount();
    }
    if (activeType == BmsTypeDaly) {
        return dalyBms.getTempCount();
    }
    return 0;
}

int16_t BluetoothBms::getTempCelsius(uint8_t index) const {
    const uint8_t activeType = getActiveType();
    if (activeType == BmsTypeJbd) {
        return jbdBms.getNtcTempCelsius(index);
    }
    if (activeType == BmsTypeDaly) {
        return dalyBms.getTempCelsius(index);
    }
    return 0;
}

uint16_t BluetoothBms::getCellVoltageMilliVolts(uint8_t index) const {
    const uint8_t activeType = getActiveType();
    if (activeType == BmsTypeJbd) {
        return jbdBms.getCellVoltageMilliVolts(index);
    }
    if (activeType == BmsTypeDaly) {
        return dalyBms.getCellVoltageMilliVolts(index);
    }
    return 0;
}

uint16_t BluetoothBms::getCellMinMilliVolts() const {
    const uint8_t activeType = getActiveType();
    if (activeType == BmsTypeJbd) {
        return jbdBms.getCellMinMilliVolts();
    }
    if (activeType == BmsTypeDaly) {
        return dalyBms.getCellMinMilliVolts();
    }
    return 0;
}

uint16_t BluetoothBms::getCellMaxMilliVolts() const {
    const uint8_t activeType = getActiveType();
    if (activeType == BmsTypeJbd) {
        return jbdBms.getCellMaxMilliVolts();
    }
    if (activeType == BmsTypeDaly) {
        return dalyBms.getCellMaxMilliVolts();
    }
    return 0;
}

uint16_t BluetoothBms::getCellDeltaMilliVolts() const {
    const uint8_t activeType = getActiveType();
    if (activeType == BmsTypeJbd) {
        return jbdBms.getCellDeltaMilliVolts();
    }
    if (activeType == BmsTypeDaly) {
        return dalyBms.getCellDeltaMilliVolts();
    }
    return 0;
}

uint8_t BluetoothBms::getActiveType() const {
    return settings.getBmsType();
}
