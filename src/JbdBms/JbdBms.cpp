#include "JbdBms.h"
#include "../config.h"
#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEUtils.h>
#include <BLE2902.h>

static JbdBms* s_jbdBms = nullptr;

void onNotifyCallback(BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
    if (s_jbdBms && pData && length > 0) {
        s_jbdBms->onNotify(pData, length);
    }
}

JbdBms::JbdBms() {
    pClient_ = nullptr;
    pCharRx_ = nullptr;
    pCharTx_ = nullptr;
    state_ = Idle;
    connected_ = false;
    hasData_ = false;
    lastConnectAttempt_ = 0;
    lastRequestMillis_ = 0;
    rxLen_ = 0;
    packVoltageMilliVolts_ = 0;
    packCurrentMilliAmps_ = 0;
    socPercent_ = 0;
    cellCount_ = 0;
    ntcCount_ = 0;
    cycleCount_ = 0;
    designCapacityMahl_ = 0;
    balanceCapacityMahl_ = 0;
    chgFetEnabled_ = 0;
    dsgFetEnabled_ = 0;
    currentErrors_ = 0;
    for (int i = 0; i < JBD_MAX_NTC; i++) {
        ntcTempsCelsius_[i] = 0;
    }
}

void JbdBms::init() {
    s_jbdBms = this;
    // BLEDevice::init() already called by Xctod
    pClient_ = BLEDevice::createClient();
    if (!pClient_) {
        DEBUG_PRINTLN("[JBD] ERROR: createClient failed");
        return;
    }
    state_ = Idle;
    DEBUG_PRINTLN("[JBD] Init OK, address: " JBD_BMS_BLE_ADDRESS);
}

void JbdBms::update() {
    switch (state_) {
        case Idle:
            if (millis() - lastConnectAttempt_ >= CONNECT_RETRY_MS) {
                lastConnectAttempt_ = millis();
                DEBUG_PRINTLN("[JBD] Connecting...");
                state_ = Connecting;
            }
            break;

        case Connecting: {
            BLEAddress addr(JBD_BMS_BLE_ADDRESS);
            if (pClient_->connect(addr)) {
                connected_ = true;
                DEBUG_PRINTLN("[JBD] Connected");
                state_ = Connected;
            } else {
                connected_ = false;
                DEBUG_PRINTLN("[JBD] Connect failed, will retry");
                state_ = Idle;
            }
            break;
        }

        case Connected: {
            BLERemoteService* pService = pClient_->getService(JBD_SERVICE_UUID);
            if (!pService) {
                DEBUG_PRINTLN("[JBD] Service FF00 not found");
                pClient_->disconnect();
                connected_ = false;
                state_ = Idle;
                break;
            }
            pCharRx_ = pService->getCharacteristic(JBD_CHAR_UUID_RX);
            pCharTx_ = pService->getCharacteristic(JBD_CHAR_UUID_TX);
            if (!pCharRx_ || !pCharTx_) {
                DEBUG_PRINTLN("[JBD] Characteristics not found");
                pClient_->disconnect();
                connected_ = false;
                state_ = Idle;
                break;
            }
            pCharRx_->registerForNotify(onNotifyCallback);
            state_ = Subscribed;
            lastRequestMillis_ = millis();
            DEBUG_PRINTLN("[JBD] Subscribed to FF01");
            break;
        }

        case Subscribed:
            if (!pClient_->isConnected()) {
                connected_ = false;
                state_ = Idle;
                pCharRx_ = nullptr;
                pCharTx_ = nullptr;
                DEBUG_PRINTLN("[JBD] Disconnected");
                break;
            }
            processRxBuffer();
            if (millis() - lastRequestMillis_ >= REQUEST_INTERVAL_MS) {
                lastRequestMillis_ = millis();
                sendBasicInfoRequest();
            }
            break;
    }
}

void JbdBms::onNotify(uint8_t* data, size_t len) {
    if (rxLen_ + len > JBD_RX_BUFFER_SIZE) {
        rxLen_ = 0;
    }
    for (size_t i = 0; i < len; i++) {
        rxBuffer_[rxLen_++] = data[i];
    }
}

void JbdBms::processRxBuffer() {
    while (rxLen_ >= 5) {
        size_t start = 0;
        // Optional BLE wrapper: some JBD modules send FF AA before the DD frame
        if (rxLen_ >= 2 && rxBuffer_[0] == 0xFF && rxBuffer_[1] == 0xAA) {
            start = 2;
        }
        while (start < rxLen_ && rxBuffer_[start] != JBD_FRAME_START) {
            start++;
        }
        if (start >= rxLen_) {
            rxLen_ = 0;
            return;
        }
        if (start > 0) {
            memmove(rxBuffer_, rxBuffer_ + start, rxLen_ - start);
            rxLen_ -= start;
        }
        if (rxLen_ < 5) {
            return;
        }
        size_t endIdx = 0;
        for (size_t i = 1; i < rxLen_; i++) {
            if (rxBuffer_[i] == JBD_FRAME_END) {
                endIdx = i;
                break;
            }
        }
        if (endIdx == 0) {
            if (rxLen_ >= JBD_RX_BUFFER_SIZE) {
                rxLen_ = 0;
            }
            return;
        }
        size_t frameLen = endIdx + 1;
        size_t payloadLen = endIdx - 1;  // bytes between DD and 77 (inclusive of checksum)
        if (payloadLen < 5) {
            memmove(rxBuffer_, rxBuffer_ + frameLen, rxLen_ - frameLen);
            rxLen_ -= frameLen;
            continue;
        }
        const uint8_t* payload = rxBuffer_ + 1;
        uint16_t recvChecksum = (uint16_t)payload[payloadLen - 2] | ((uint16_t)payload[payloadLen - 1] << 8);
        uint32_t sum = 0;
        for (size_t i = 0; i < payloadLen - 2; i++) {
            sum += payload[i];
        }
        if (((sum + recvChecksum) & 0xFFFF) != 0) {
            memmove(rxBuffer_, rxBuffer_ + frameLen, rxLen_ - frameLen);
            rxLen_ -= frameLen;
            continue;
        }
        uint8_t reg = payload[0];
        uint8_t status = payload[1];
        uint8_t dataLen = payload[2];
        if (payloadLen < 3 + (size_t)dataLen + 2) {
            memmove(rxBuffer_, rxBuffer_ + frameLen, rxLen_ - frameLen);
            rxLen_ -= frameLen;
            continue;
        }
        const uint8_t* dataPayload = payload + 3;

        printFrameHex(rxBuffer_, frameLen);

        if (reg == JBD_REG_BASIC_INFO && status == 0x00) {
            parseBasicInfo(dataPayload, dataLen);
            printBasicInfo();
        }

        memmove(rxBuffer_, rxBuffer_ + frameLen, rxLen_ - frameLen);
        rxLen_ -= frameLen;
    }
}

void JbdBms::buildReadFrame(uint8_t reg, uint8_t len, uint8_t* out, size_t* outLen) {
    uint8_t pld[3] = { JBD_CMD_READ, reg, len };
    uint32_t sum = 0;
    for (int i = 0; i < 3; i++) {
        sum += pld[i];
    }
    uint16_t checksum = (uint16_t)(0x10000 - sum);
    out[0] = JBD_FRAME_START;
    out[1] = pld[0];
    out[2] = pld[1];
    out[3] = pld[2];
    out[4] = (uint8_t)(checksum & 0xFF);
    out[5] = (uint8_t)(checksum >> 8);
    out[6] = JBD_FRAME_END;
    *outLen = 7;
}

void JbdBms::sendBasicInfoRequest() {
    if (!pCharTx_) return;
    uint8_t frame[8];
    size_t frameLen;
    buildReadFrame(JBD_REG_BASIC_INFO, 0, frame, &frameLen);
    pCharTx_->writeValue(frame, frameLen, false);
}

void JbdBms::parseBasicInfo(const uint8_t* d, size_t dataLen) {
    if (dataLen < 0x16) return;
    packVoltageMilliVolts_ = ((uint32_t)d[0] << 8 | d[1]) * 10;
    packCurrentMilliAmps_ = (int32_t)(int16_t)((uint16_t)d[2] << 8 | d[3]) * 10;
    balanceCapacityMahl_ = ((uint16_t)d[4] << 8 | d[5]) * 10;
    designCapacityMahl_ = ((uint16_t)d[6] << 8 | d[7]) * 10;
    cycleCount_ = (uint16_t)d[8] << 8 | d[9];
    if (dataLen > 0x12) socPercent_ = d[0x12];
    if (dataLen > 0x13) {
        chgFetEnabled_ = (d[0x13] >> 0) & 1;
        dsgFetEnabled_ = (d[0x13] >> 1) & 1;
    }
    if (dataLen > 0x14) cellCount_ = d[0x14];
    if (dataLen > 0x15) ntcCount_ = d[0x15];
    if (dataLen > 0x10) currentErrors_ = (uint16_t)d[0x10] << 8 | d[0x11];
    for (uint8_t i = 0; i < JBD_MAX_NTC && i < ntcCount_ && (0x16 + (i + 1) * 2) <= dataLen; i++) {
        uint16_t val = (uint16_t)d[0x16 + i * 2] << 8 | d[0x16 + i * 2 + 1];
        ntcTempsCelsius_[i] = (int16_t)((val / 10.0) - 273.15);
    }
    hasData_ = true;
}

void JbdBms::printFrameHex(const uint8_t* data, size_t len) {
    DEBUG_PRINT("[JBD] RX: ");
    for (size_t i = 0; i < len; i++) {
        if (data[i] < 0x10) DEBUG_PRINT("0");
        DEBUG_PRINT_HEX(data[i], HEX);
        DEBUG_PRINT(" ");
    }
    DEBUG_PRINTLN();
}

void JbdBms::printBasicInfo() {
    DEBUG_PRINTLN("[JBD] --- Basic Info ---");
    DEBUG_PRINT("[JBD] Pack voltage: ");
    DEBUG_PRINT(packVoltageMilliVolts_ / 1000);
    DEBUG_PRINT(".");
    DEBUG_PRINT((packVoltageMilliVolts_ % 1000) / 100);
    DEBUG_PRINTLN(" V");
    DEBUG_PRINT("[JBD] Pack current: ");
    DEBUG_PRINT(packCurrentMilliAmps_);
    DEBUG_PRINTLN(" mA");
    DEBUG_PRINT("[JBD] SoC: ");
    DEBUG_PRINT(socPercent_);
    DEBUG_PRINTLN(" %");
    DEBUG_PRINT("[JBD] Cells: ");
    DEBUG_PRINTLN(cellCount_);
    DEBUG_PRINT("[JBD] Cycle count: ");
    DEBUG_PRINTLN(cycleCount_);
    DEBUG_PRINT("[JBD] Design capacity: ");
    DEBUG_PRINT(designCapacityMahl_ / 1000);
    DEBUG_PRINTLN(" mAh");
    DEBUG_PRINT("[JBD] Balance capacity: ");
    DEBUG_PRINT(balanceCapacityMahl_ / 1000);
    DEBUG_PRINTLN(" mAh");
    DEBUG_PRINT("[JBD] FET CHG: ");
    DEBUG_PRINT(chgFetEnabled_ ? "ON" : "OFF");
    DEBUG_PRINT(" DSG: ");
    DEBUG_PRINTLN(dsgFetEnabled_ ? "ON" : "OFF");
    if (currentErrors_ != 0) {
        DEBUG_PRINT("[JBD] Errors: 0x");
        DEBUG_PRINT_HEX(currentErrors_, HEX);
        DEBUG_PRINTLN();
    }
    DEBUG_PRINT("[JBD] NTC count: ");
    DEBUG_PRINTLN(ntcCount_);
    for (uint8_t i = 0; i < ntcCount_ && i < JBD_MAX_NTC; i++) {
        DEBUG_PRINT("[JBD] NTC");
        DEBUG_PRINT(i);
        DEBUG_PRINT(": ");
        DEBUG_PRINT(ntcTempsCelsius_[i]);
        DEBUG_PRINTLN(" C");
    }
    DEBUG_PRINTLN("[JBD] -----------------");
}

int16_t JbdBms::getNtcTempCelsius(uint8_t index) const {
    if (index >= JBD_MAX_NTC || index >= ntcCount_) return 0;
    return ntcTempsCelsius_[index];
}
