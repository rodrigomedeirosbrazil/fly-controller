#include "DalyBms.h"
#include "../config.h"
#include "../Throttle/Throttle.h"
#include <BLE2902.h>
#include <BLEClient.h>
#include <BLEDevice.h>
#include <BLEUtils.h>

#define DALY_SERVICE_UUID "0000fff0-0000-1000-8000-00805f9b34fb"
#define DALY_CHAR_UUID_RX "0000fff1-0000-1000-8000-00805f9b34fb"
#define DALY_CHAR_UUID_TX "0000fff2-0000-1000-8000-00805f9b34fb"

static const uint8_t DALY_READ_FN = 0x03;
static const uint8_t DALY_STATUS_REG_COUNT_62 = 62;
static const uint8_t DALY_STATUS_DATA_LEN_62 = DALY_STATUS_REG_COUNT_62 * 2;

static DalyBms* s_dalyBms = nullptr;

static void onDalyNotifyCallback(BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
    (void)pChar;
    (void)isNotify;
    if (s_dalyBms != nullptr && pData != nullptr && length > 0) {
        s_dalyBms->onNotify(pData, length);
    }
}

DalyBms::DalyBms()
    : pClient_(nullptr), pCharRx_(nullptr), pCharTx_(nullptr),
      state_(Idle), enabled_(false), connected_(false), hasData_(false), hasCellData_(false),
      lastConnectAttempt_(0), lastRequestMillis_(0), rxLen_(0),
      packVoltageMilliVolts_(0), packCurrentMilliAmps_(0), socPercent_(0),
      remainingCapacityMilliAh_(0), cycleCount_(0), cellCount_(0), tempCount_(0),
      balanceEnabled_(false), chargeEnabled_(false), dischargeEnabled_(false) {
    memset(rxBuffer_, 0, sizeof(rxBuffer_));
    memset(tempsCelsius_, 0, sizeof(tempsCelsius_));
    memset(cellVoltagesMv_, 0, sizeof(cellVoltagesMv_));
}

void DalyBms::init() {
    s_dalyBms = this;
    pClient_ = BLEDevice::createClient();
    if (pClient_ == nullptr) {
        DEBUG_PRINTLN("[Daly] ERROR: createClient failed");
        return;
    }
    state_ = Idle;
}

void DalyBms::setEnabled(bool enabled) {
    enabled_ = enabled;
    if (!enabled_) {
        resetConnection();
    }
}

void DalyBms::setMacAddress(const String& macAddress) {
    macAddress_ = macAddress;
    macAddress_.trim();
}

void DalyBms::update() {
    switch (state_) {
        case Idle:
            if (!enabled_ || macAddress_.length() < 17 || throttle.isArmed()) {
                break;
            }
            if (millis() - lastConnectAttempt_ >= CONNECT_RETRY_MS) {
                lastConnectAttempt_ = millis();
                state_ = Connecting;
                DEBUG_PRINTLN("[Daly] Connecting...");
            }
            break;

        case Connecting: {
            BLEAddress addr(macAddress_.c_str());
            if (pClient_ != nullptr && pClient_->connect(addr)) {
                connected_ = true;
                rxLen_ = 0;
                state_ = Connected;
                DEBUG_PRINTLN("[Daly] Connected");
            } else {
                DEBUG_PRINTLN("[Daly] Connection failed");
                resetConnection();
            }
            break;
        }

        case Connected: {
            BLERemoteService* pService = pClient_->getService(DALY_SERVICE_UUID);
            if (pService == nullptr) {
                DEBUG_PRINTLN("[Daly] Service FFF0 not found");
                resetConnection();
                break;
            }

            pCharRx_ = pService->getCharacteristic(DALY_CHAR_UUID_RX);
            pCharTx_ = pService->getCharacteristic(DALY_CHAR_UUID_TX);
            if (pCharRx_ == nullptr || pCharTx_ == nullptr) {
                DEBUG_PRINTLN("[Daly] Characteristics FFF1/FFF2 not found");
                resetConnection();
                break;
            }

            pCharRx_->registerForNotify(onDalyNotifyCallback);
            BLERemoteDescriptor* pCccd = pCharRx_->getDescriptor(BLEUUID((uint16_t)0x2902));
            if (pCccd != nullptr) {
                uint8_t notifyOn[2] = {0x01, 0x00};
                pCccd->writeValue(notifyOn, 2, true);
            }

            state_ = Subscribed;
            lastRequestMillis_ = 0;
            DEBUG_PRINTLN("[Daly] Notify subscribed");
            break;
        }

        case Subscribed:
            if (pClient_ == nullptr || !pClient_->isConnected()) {
                DEBUG_PRINTLN("[Daly] Disconnected");
                resetConnection();
                break;
            }
            processRxBuffer();
            if (millis() - lastRequestMillis_ >= REQUEST_INTERVAL_MS) {
                lastRequestMillis_ = millis();
                sendStatusRequest();
            }
            break;
    }
}

int16_t DalyBms::getTempCelsius(uint8_t index) const {
    if (index >= MAX_TEMPS || index >= tempCount_) return 0;
    return tempsCelsius_[index];
}

uint16_t DalyBms::getCellVoltageMilliVolts(uint8_t index) const {
    if (index >= MAX_CELLS || index >= cellCount_) return 0;
    return cellVoltagesMv_[index];
}

uint16_t DalyBms::getCellMinMilliVolts() const {
    if (!hasCellData_ || cellCount_ == 0) return 0;
    uint16_t minMv = 0xFFFF;
    for (uint8_t i = 0; i < cellCount_ && i < MAX_CELLS; i++) {
        if (cellVoltagesMv_[i] > 0 && cellVoltagesMv_[i] < minMv) {
            minMv = cellVoltagesMv_[i];
        }
    }
    return minMv == 0xFFFF ? 0 : minMv;
}

uint16_t DalyBms::getCellMaxMilliVolts() const {
    if (!hasCellData_ || cellCount_ == 0) return 0;
    uint16_t maxMv = 0;
    for (uint8_t i = 0; i < cellCount_ && i < MAX_CELLS; i++) {
        if (cellVoltagesMv_[i] > maxMv) {
            maxMv = cellVoltagesMv_[i];
        }
    }
    return maxMv;
}

uint16_t DalyBms::getCellDeltaMilliVolts() const {
    if (!hasCellData_) return 0;
    uint16_t minMv = getCellMinMilliVolts();
    uint16_t maxMv = getCellMaxMilliVolts();
    return maxMv >= minMv ? (maxMv - minMv) : 0;
}

void DalyBms::onNotify(uint8_t* data, size_t len) {
    if (rxLen_ + len > RX_BUFFER_SIZE) {
        rxLen_ = 0;
        DEBUG_PRINTLN("[Daly] RX buffer overflow");
    }
    memcpy(rxBuffer_ + rxLen_, data, len);
    rxLen_ += len;
}

void DalyBms::resetConnection() {
    if (pClient_ != nullptr && pClient_->isConnected()) {
        pClient_->disconnect();
    }
    connected_ = false;
    pCharRx_ = nullptr;
    pCharTx_ = nullptr;
    rxLen_ = 0;
    state_ = Idle;
}

void DalyBms::sendStatusRequest() {
    if (pCharTx_ == nullptr || pClient_ == nullptr || !pClient_->isConnected()) {
        return;
    }

    uint8_t frame[8] = {0xD2, DALY_READ_FN, 0x00, 0x00, 0x00, DALY_STATUS_REG_COUNT_62, 0x00, 0x00};
    uint16_t crc = crc16Modbus(frame, 6);
    frame[6] = (uint8_t)(crc & 0xFF);
    frame[7] = (uint8_t)(crc >> 8);
    pCharTx_->writeValue(frame, sizeof(frame), false);
}

void DalyBms::processRxBuffer() {
    while (rxLen_ >= 5) {
        if (rxBuffer_[0] != 0xD2) {
            size_t skip = 0;
            while (skip < rxLen_ && rxBuffer_[skip] != 0xD2) skip++;
            if (skip >= rxLen_) {
                rxLen_ = 0;
                return;
            }
            memmove(rxBuffer_, rxBuffer_ + skip, rxLen_ - skip);
            rxLen_ -= skip;
            continue;
        }

        uint8_t dataLen = rxBuffer_[2];
        size_t frameLen = (size_t)3 + dataLen + 2;
        if (frameLen > RX_BUFFER_SIZE) {
            rxLen_ = 0;
            return;
        }
        if (rxLen_ < frameLen) {
            return;
        }

        uint16_t expectedCrc = crc16Modbus(rxBuffer_, frameLen - 2);
        uint16_t receivedCrc = (uint16_t)rxBuffer_[frameLen - 2] | ((uint16_t)rxBuffer_[frameLen - 1] << 8);
        if (expectedCrc == receivedCrc) {
            parseStatusFrame(rxBuffer_, frameLen);
            memmove(rxBuffer_, rxBuffer_ + frameLen, rxLen_ - frameLen);
            rxLen_ -= frameLen;
            continue;
        }

        DEBUG_PRINTLN("[Daly] Invalid CRC");
        memmove(rxBuffer_, rxBuffer_ + 1, rxLen_ - 1);
        rxLen_ -= 1;
    }
}

void DalyBms::parseStatusFrame(const uint8_t* frame, size_t frameLen) {
    if (frameLen < (size_t)3 + DALY_STATUS_DATA_LEN_62 + 2) {
        return;
    }
    if (frame[1] != DALY_READ_FN || frame[2] < DALY_STATUS_DATA_LEN_62) {
        return;
    }

    auto get16 = [&](size_t index) -> uint16_t {
        if (index + 1 >= frameLen) return 0;
        return (uint16_t(frame[index]) << 8) | uint16_t(frame[index + 1]);
    };

    uint8_t decodedCellCount = (uint8_t)min<uint16_t>(get16(101), MAX_CELLS);
    uint8_t decodedTempCount = (uint8_t)min<uint16_t>(get16(103), MAX_TEMPS);

    packVoltageMilliVolts_ = (uint32_t)get16(83) * 100UL;
    packCurrentMilliAmps_ = ((int32_t)get16(85) - 30000) * 100L;
    socPercent_ = (uint8_t)((get16(87) + 5) / 10);
    remainingCapacityMilliAh_ = (uint32_t)get16(99) * 100UL;
    cycleCount_ = get16(105);
    cellCount_ = decodedCellCount;
    tempCount_ = decodedTempCount;
    balanceEnabled_ = (get16(107) == 0x0001);
    chargeEnabled_ = (get16(109) == 0x0001);
    dischargeEnabled_ = (get16(111) == 0x0001);

    memset(cellVoltagesMv_, 0, sizeof(cellVoltagesMv_));
    memset(tempsCelsius_, 0, sizeof(tempsCelsius_));

    for (uint8_t i = 0; i < cellCount_; i++) {
        cellVoltagesMv_[i] = get16(3 + (i * 2));
    }
    for (uint8_t i = 0; i < tempCount_; i++) {
        tempsCelsius_[i] = (int16_t)get16(67 + (i * 2)) - 40;
    }

    hasData_ = true;
    hasCellData_ = (cellCount_ > 0);
}

uint16_t DalyBms::crc16Modbus(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if ((crc & 0x0001U) != 0U) {
                crc = (crc >> 1) ^ 0xA001U;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}
