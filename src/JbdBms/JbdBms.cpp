#include "JbdBms.h"
#include "../config.h"
#include "../Settings/Settings.h"
#include "../Throttle/Throttle.h"
#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEUtils.h>
#include <BLE2902.h>

struct BleFrame {
    uint8_t data[16];
    size_t  len;
};

static JbdBms* s_jbdBms = nullptr;

// ---------------------------------------------------------------------------
// Global notify callback
// ---------------------------------------------------------------------------
void onNotifyCallback(BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
    if (s_jbdBms && pData && length > 0) {
        s_jbdBms->onNotify(pData, length);
    }
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
JbdBms::JbdBms()
    : pClient_(nullptr), pCharRx_(nullptr), pCharTx_(nullptr),
      txQueue_(nullptr), txTaskHandle_(nullptr),
      state_(Idle), connected_(false), hasData_(false),
      lastConnectAttempt_(0), lastRequestMillis_(0), rxLen_(0),
      packVoltageMilliVolts_(0), packCurrentMilliAmps_(0), socPercent_(0),
      cellCount_(0), ntcCount_(0), cycleCount_(0),
      designCapacityMahl_(0), balanceCapacityMahl_(0),
      chgFetEnabled_(0), dsgFetEnabled_(0), currentErrors_(0),
      hasCellData_(false), requestCells_(false)
{
    memset(ntcTempsCelsius_, 0, sizeof(ntcTempsCelsius_));
    memset(cellVoltagesMv_,  0, sizeof(cellVoltagesMv_));
    memset(rxBuffer_, 0, sizeof(rxBuffer_));
}

// ---------------------------------------------------------------------------
// init
// ---------------------------------------------------------------------------
void JbdBms::init() {
    s_jbdBms = this;
    // BLEDevice::init() must have been called by the main system
    pClient_ = BLEDevice::createClient();
    if (!pClient_) {
        DEBUG_PRINTLN("[JBD] ERROR: createClient failed");
        return;
    }
    txQueue_ = xQueueCreate(4, sizeof(BleFrame));
    xTaskCreate(txTask, "jbd_tx", 2048, this, 1, &txTaskHandle_);
    state_ = Idle;
    DEBUG_PRINTLN("[JBD] Init OK");
}

// ---------------------------------------------------------------------------
// resetConnection — clears BLE state for new attempt
// ---------------------------------------------------------------------------
void JbdBms::resetConnection() {
    if (pClient_ && pClient_->isConnected()) {
        pClient_->disconnect();
    }
    if (txQueue_ != nullptr) {
        xQueueReset(txQueue_);
    }
    connected_ = false;
    pCharRx_   = nullptr;
    pCharTx_   = nullptr;
    rxLen_     = 0;
    state_     = Idle;
}

// ---------------------------------------------------------------------------
// txTask — FreeRTOS task that performs BLE writes (avoids blocking the main loop)
// ---------------------------------------------------------------------------
void JbdBms::txTask(void* arg) {
    JbdBms* self = static_cast<JbdBms*>(arg);
    BleFrame frame;
    for (;;) {
        if (xQueueReceive(self->txQueue_, &frame, portMAX_DELAY)) {
            if (self->pCharTx_ && self->pClient_ && self->pClient_->isConnected()) {
                self->pCharTx_->writeValue(frame.data, frame.len, false);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// update — state machine, call from loop()
// ---------------------------------------------------------------------------
void JbdBms::update() {
    switch (state_) {

        // ------------------------------------------------------------------
        case Idle: {
            extern Settings settings;
            String mac = settings.getJbdBmsMac();
            if (!settings.getJbdBmsEnabled() || mac.length() < 17) {
                break;  // Do not attempt connection
            }
            if (throttle.isArmed()) {
                break;  // Do not attempt connection while motor is armed
            }
            if (millis() - lastConnectAttempt_ >= CONNECT_RETRY_MS) {
                lastConnectAttempt_ = millis();
                DEBUG_PRINTLN("[JBD] Connecting...");
                state_ = Connecting;
            }
            break;
        }

        // ------------------------------------------------------------------
        case Connecting: {
            extern Settings settings;
            String mac = settings.getJbdBmsMac();
            if (mac.length() < 17) {
                resetConnection();
                break;
            }
            BLEAddress addr(mac.c_str());
            if (pClient_->connect(addr)) {
                connected_ = true;
                rxLen_     = 0;
                DEBUG_PRINTLN("[JBD] Connected");
                state_ = Connected;
            } else {
                DEBUG_PRINTLN("[JBD] Connection failed, retrying...");
                resetConnection();
            }
            break;
        }

        // ------------------------------------------------------------------
        case Connected: {
            // Locate the JBD service (FF00)
            BLERemoteService* pService = pClient_->getService(JBD_SERVICE_UUID);
            if (!pService) {
                DEBUG_PRINTLN("[JBD] Service FF00 not found");
                resetConnection();
                break;
            }

            // Locate characteristics
            pCharRx_ = pService->getCharacteristic(JBD_CHAR_UUID_RX); // FF01 notify
            pCharTx_ = pService->getCharacteristic(JBD_CHAR_UUID_TX); // FF02 write

            if (!pCharRx_ || !pCharTx_) {
                DEBUG_PRINTLN("[JBD] Characteristics FF01/FF02 not found");
                resetConnection();
                break;
            }

            // FIX: ESP32-C3 needs to write the CCCD descriptor manually.
            // registerForNotify alone may not enable notifications on C3.
            pCharRx_->registerForNotify(onNotifyCallback);

            // Write 0x0100 to descriptor 0x2902 to enable notify explicitly
            BLERemoteDescriptor* pCccd = pCharRx_->getDescriptor(BLEUUID((uint16_t)0x2902));
            if (pCccd) {
                uint8_t notifyOn[2] = {0x01, 0x00};
                pCccd->writeValue(notifyOn, 2, true);
                DEBUG_PRINTLN("[JBD] CCCD written manually (0x0100)");
            } else {
                DEBUG_PRINTLN("[JBD] WARNING: descriptor 0x2902 not found — notify may not work");
            }

            state_             = Subscribed;
            lastRequestMillis_ = 0; // force immediate send
            DEBUG_PRINTLN("[JBD] Subscribed to FF02, ready for reads");
            break;
        }

        // ------------------------------------------------------------------
        case Subscribed:
            if (!pClient_->isConnected()) {
                DEBUG_PRINTLN("[JBD] Unexpectedly disconnected");
                resetConnection();
                break;
            }

            // Process frames accumulated in RX buffer
            processRxBuffer();

            // Send alternating requests: 0x03 (basic) and 0x04 (cells)
            if (millis() - lastRequestMillis_ >= REQUEST_INTERVAL_MS) {
                lastRequestMillis_ = millis();
                if (requestCells_) {
                    sendCellVoltageRequest();
                } else {
                    sendBasicInfoRequest();
                }
                requestCells_ = !requestCells_;
            }
            break;
    }
}

// ---------------------------------------------------------------------------
// onNotify — called by BLE stack ISR-like callback
// ---------------------------------------------------------------------------
void JbdBms::onNotify(uint8_t* data, size_t len) {
    if (rxLen_ + len > JBD_RX_BUFFER_SIZE) {
        // Overflow: discard buffer to avoid garbage accumulation
        DEBUG_PRINTLN("[JBD] RX buffer overflow, discarding");
        rxLen_ = 0;
    }
    memcpy(rxBuffer_ + rxLen_, data, len);
    rxLen_ += len;
}

// ---------------------------------------------------------------------------
// processRxBuffer — extract and validate JBD frames from accumulated buffer
//
// This BMS (SP14S004 V1.0) sends responses with "FF AA" wrapper before DD frame:
//   FF AA | DD | REG | STATUS | LEN | DATA[LEN] | CHK_H | CHK_L | 77
//
// BLE limits 20 bytes per packet, so the response may arrive in multiple chunks.
// onNotify() accumulates all in rxBuffer_ and here we assemble the full frame.
//
// Checksum (reception): 0x10000 - (STATUS + LEN + DATA[...])
// ---------------------------------------------------------------------------
void JbdBms::processRxBuffer() {
    while (rxLen_ > 0) {

        // --- Discard FF AA wrapper if present at start ---
        if (rxLen_ >= 2 && rxBuffer_[0] == 0xFF && rxBuffer_[1] == 0xAA) {
            memmove(rxBuffer_, rxBuffer_ + 2, rxLen_ - 2);
            rxLen_ -= 2;
            continue;
        }

        // --- Look for start byte 0xDD ---
        if (rxBuffer_[0] != JBD_FRAME_START) {
            size_t skip = 0;
            while (skip < rxLen_ && rxBuffer_[skip] != JBD_FRAME_START) skip++;
            if (skip >= rxLen_) { rxLen_ = 0; return; }
            memmove(rxBuffer_, rxBuffer_ + skip, rxLen_ - skip);
            rxLen_ -= skip;
            continue;
        }

        // --- Need at least 4 bytes to read LEN ---
        if (rxLen_ < 4) return;

        uint8_t reg     = rxBuffer_[1];
        uint8_t status  = rxBuffer_[2];
        uint8_t dataLen = rxBuffer_[3];

        // Frame completo: DD(1) + REG(1) + STATUS(1) + LEN(1) + DATA(N) + CHK_H(1) + CHK_L(1) + 77(1)
        size_t frameLen = (size_t)dataLen + 7;

        // Wait for full frame to arrive (may come in multiple BLE packets)
        if (rxLen_ < frameLen) return;

        // --- Verify end byte 0x77 ---
        if (rxBuffer_[frameLen - 1] != JBD_FRAME_END) {
            DEBUG_PRINTLN("[JBD] Invalid frame end, resyncing...");
            memmove(rxBuffer_, rxBuffer_ + 1, rxLen_ - 1);
            rxLen_ -= 1;
            continue;
        }

        // --- Raw frame log ---
        DEBUG_PRINT("[JBD] RX: ");
        printFrameHex(rxBuffer_, frameLen);

        // --- Valida checksum: 0x10000 - (STATUS + LEN + DATA[...]) ---
        uint32_t sum = (uint32_t)status + dataLen;
        for (size_t i = 0; i < (size_t)dataLen; i++) sum += rxBuffer_[4 + i];
        uint16_t recvChk = ((uint16_t)rxBuffer_[4 + dataLen] << 8) | rxBuffer_[5 + dataLen];

        if (((sum + recvChk) & 0xFFFF) != 0) {
            DEBUG_PRINT("[JBD] Invalid checksum sum=0x");
            DEBUG_PRINT_HEX((uint16_t)(sum & 0xFFFF), HEX);
            DEBUG_PRINT(" chk=0x");
            DEBUG_PRINT_HEX(recvChk, HEX);
            DEBUG_PRINTLN();
            memmove(rxBuffer_, rxBuffer_ + 1, rxLen_ - 1);
            rxLen_ -= 1;
            continue;
        }

        // --- Valid frame: process ---
        const uint8_t* dataPayload = rxBuffer_ + 4;

        if (status == 0x00) {
            if (reg == JBD_REG_BASIC_INFO) {
                parseBasicInfo(dataPayload, dataLen);
                printBasicInfo();
            } else if (reg == JBD_REG_CELL_VOLTAGES) {
                parseCellVoltages(dataPayload, dataLen);
                printCellVoltages();
            }
        } else {
            DEBUG_PRINT("[JBD] BMS error: reg=0x");
            DEBUG_PRINT_HEX(reg, HEX);
            DEBUG_PRINT(" status=0x");
            DEBUG_PRINT_HEX(status, HEX);
            DEBUG_PRINTLN();
        }

        // Consume frame from buffer
        memmove(rxBuffer_, rxBuffer_ + frameLen, rxLen_ - frameLen);
        rxLen_ -= frameLen;
    }
}

// ---------------------------------------------------------------------------
// buildReadFrame — build JBD read frame (7 bytes)
//
// Format:  DD | A5 | REG | 00 | CHK_H | CHK_L | 77
// Checksum: 0x10000 - REG  (REG byte only, no CMD or LEN)
//
// Confirmed by canonical frame: DD A5 03 00 FF FD 77
//   0x10000 - 0x03 = 0xFFFD ✓
// ---------------------------------------------------------------------------
void JbdBms::buildReadFrame(uint8_t reg, uint8_t* out, size_t* outLen) {
    uint16_t checksum = (uint16_t)(0x10000 - reg);  // REG only

    out[0] = JBD_FRAME_START;
    out[1] = JBD_CMD_READ;
    out[2] = reg;
    out[3] = 0x00;                          // LEN = 0
    out[4] = (uint8_t)(checksum >> 8);      // CHK_H
    out[5] = (uint8_t)(checksum & 0xFF);    // CHK_L
    out[6] = JBD_FRAME_END;
    *outLen = 7;
}

// ---------------------------------------------------------------------------
// buildWriteFrame — build JBD write frame
// Format: DD | 5A | REG | LEN | DATA... | CHK_H | CHK_L | 77
// Checksum: 0x10000 - (REG + LEN + DATA[...])
// ---------------------------------------------------------------------------
void JbdBms::buildWriteFrame(uint8_t reg, const uint8_t* data, uint8_t dataLen, uint8_t* out, size_t* outLen) {
    uint32_t sum = (uint32_t)reg + dataLen;
    for (uint8_t i = 0; i < dataLen; i++) sum += data[i];
    uint16_t checksum = (uint16_t)(0x10000 - sum);

    out[0] = JBD_FRAME_START;
    out[1] = JBD_CMD_WRITE;
    out[2] = reg;
    out[3] = dataLen;
    for (uint8_t i = 0; i < dataLen; i++) out[4 + i] = data[i];
    out[4 + dataLen] = (uint8_t)(checksum >> 8);
    out[5 + dataLen] = (uint8_t)(checksum & 0xFF);
    out[6 + dataLen] = JBD_FRAME_END;
    *outLen = 7 + dataLen;
}

// ---------------------------------------------------------------------------
// sendLoginRequest — send default JBD password (0x00000000) to reg 0x00
// Required on some firmwares before accepting reads
// ---------------------------------------------------------------------------
void JbdBms::sendLoginRequest() {
    if (!pCharTx_) return;
    uint8_t password[4] = {0x00, 0x00, 0x00, 0x00};
    uint8_t frame[16];
    size_t  frameLen;
    buildWriteFrame(JBD_REG_LOGIN, password, 4, frame, &frameLen);
    DEBUG_PRINT("[JBD] LOGIN TX: ");
    printFrameHex(frame, frameLen);
    pCharTx_->writeValue(frame, frameLen, false);
}


// ---------------------------------------------------------------------------
void JbdBms::sendBasicInfoRequest() {
    if (!txQueue_) return;
    BleFrame frame;
    buildReadFrame(JBD_REG_BASIC_INFO, frame.data, &frame.len);
    DEBUG_PRINT("[JBD] TX: ");
    printFrameHex(frame.data, frame.len);
    xQueueSend(txQueue_, &frame, 0);
}

// ---------------------------------------------------------------------------
// sendCellVoltageRequest
// ---------------------------------------------------------------------------
void JbdBms::sendCellVoltageRequest() {
    if (!txQueue_) return;
    BleFrame frame;
    buildReadFrame(JBD_REG_CELL_VOLTAGES, frame.data, &frame.len);
    DEBUG_PRINT("[JBD] TX cells: ");
    printFrameHex(frame.data, frame.len);
    xQueueSend(txQueue_, &frame, 0);
}

// Decode register 0x03 (Basic Info)
//
// JBD standard protocol, offset within DATA field:
//   0-1   Total voltage         (unit: 10mV)
//   2-3   Current               (unit: 10mA, signed)
//   4-5   Remaining capacity     (unit: 10mAh)
//   6-7   Nominal capacity       (unit: 10mAh)
//   8-9   Charge cycles
//  10-11  Production date (BCD YYYYMMDD packed)
//  12-13  Balance bitmask (cells 1-16)
//  14-15  Balance bitmask (cells 17-32)
//  16-17  Protection/error flags
//    18   Software version
//    19   SoC (%)
//    20   FET status (bit0=CHG, bit1=DSG)
//    21   Number of cells in series
//    22   Number of NTCs
//  23+    NTC temperature (2 bytes each, Kelvin*10, big-endian)
// ---------------------------------------------------------------------------
void JbdBms::parseBasicInfo(const uint8_t* d, size_t dataLen) {
    if (dataLen < 23) {
        DEBUG_PRINT("[JBD] dataLen too short: ");
        DEBUG_PRINTLN(dataLen);
        return;
    }

    packVoltageMilliVolts_ = ((uint32_t)d[0] << 8 | d[1]) * 10;
    packCurrentMilliAmps_  = (int32_t)(int16_t)((uint16_t)d[2] << 8 | d[3]) * 10;
    balanceCapacityMahl_   = ((uint32_t)d[4] << 8 | d[5]) * 10;
    designCapacityMahl_    = ((uint32_t)d[6] << 8 | d[7]) * 10;
    cycleCount_            = (uint16_t)d[8] << 8 | d[9];
    currentErrors_         = (uint16_t)d[16] << 8 | d[17];
    socPercent_            = d[19];
    chgFetEnabled_         = (d[20] >> 0) & 1;
    dsgFetEnabled_         = (d[20] >> 1) & 1;
    cellCount_             = d[21];
    ntcCount_              = d[22];

    for (uint8_t i = 0; i < ntcCount_ && i < JBD_MAX_NTC; i++) {
        size_t off = 23 + i * 2;
        if (off + 1 >= dataLen) break;
        uint16_t raw = (uint16_t)d[off] << 8 | d[off + 1];
        ntcTempsCelsius_[i] = (int16_t)((int32_t)raw - 2731) / 10;
    }

    hasData_ = true;
}

// ---------------------------------------------------------------------------
// parseCellVoltages — decode register 0x04
//
// DATA: 2 bytes per cell, big-endian, direct value in mV
//   d[0-1] = cell 1 (mV)
//   d[2-3] = cell 2 (mV)
//   ...
// ---------------------------------------------------------------------------
void JbdBms::parseCellVoltages(const uint8_t* d, size_t dataLen) {
    uint8_t count = dataLen / 2;
    if (count > JBD_MAX_CELLS) count = JBD_MAX_CELLS;
    // Update cellCount_ if not yet received from 0x03
    if (cellCount_ == 0) cellCount_ = count;
    for (uint8_t i = 0; i < count; i++) {
        cellVoltagesMv_[i] = (uint16_t)d[i * 2] << 8 | d[i * 2 + 1];
    }
    hasCellData_ = true;
}

// ---------------------------------------------------------------------------
// printCellVoltages
// ---------------------------------------------------------------------------
void JbdBms::printCellVoltages() {
    uint8_t count = cellCount_ > 0 ? cellCount_ : 0;
    DEBUG_PRINTLN("[JBD] ===== Cell Voltages =====");
    uint16_t vmin = 0xFFFF, vmax = 0;
    for (uint8_t i = 0; i < count && i < JBD_MAX_CELLS; i++) {
        uint16_t v = cellVoltagesMv_[i];
        DEBUG_PRINT("[JBD] C");
        if (i + 1 < 10) DEBUG_PRINT("0");
        DEBUG_PRINT(i + 1);
        DEBUG_PRINT(": ");
        DEBUG_PRINT(v / 1000);
        DEBUG_PRINT(".");
        // 3 decimal places
        uint16_t frac = v % 1000;
        if (frac < 100) DEBUG_PRINT("0");
        if (frac < 10)  DEBUG_PRINT("0");
        DEBUG_PRINT(frac);
        DEBUG_PRINTLN(" V");
        if (v < vmin) vmin = v;
        if (v > vmax) vmax = v;
    }
    if (count > 0) {
        DEBUG_PRINT("[JBD] Min: ");
        DEBUG_PRINT(vmin);
        DEBUG_PRINT(" mV  Max: ");
        DEBUG_PRINT(vmax);
        DEBUG_PRINT(" mV  Delta: ");
        DEBUG_PRINT(vmax - vmin);
        DEBUG_PRINTLN(" mV");
    }
    DEBUG_PRINTLN("[JBD] ==========================");
}

// ---------------------------------------------------------------------------
// getCellVoltageMilliVolts
// ---------------------------------------------------------------------------
uint16_t JbdBms::getCellVoltageMilliVolts(uint8_t index) const {
    if (index >= JBD_MAX_CELLS || index >= cellCount_) return 0;
    return cellVoltagesMv_[index];
}

uint16_t JbdBms::getCellMinMilliVolts() const {
    if (!hasCellData_ || cellCount_ == 0) return 0;
    uint16_t vmin = 0xFFFF;
    for (uint8_t i = 0; i < cellCount_ && i < JBD_MAX_CELLS; i++)
        if (cellVoltagesMv_[i] < vmin) vmin = cellVoltagesMv_[i];
    return vmin;
}

uint16_t JbdBms::getCellMaxMilliVolts() const {
    if (!hasCellData_ || cellCount_ == 0) return 0;
    uint16_t vmax = 0;
    for (uint8_t i = 0; i < cellCount_ && i < JBD_MAX_CELLS; i++)
        if (cellVoltagesMv_[i] > vmax) vmax = cellVoltagesMv_[i];
    return vmax;
}

uint16_t JbdBms::getCellDeltaMilliVolts() const {
    if (!hasCellData_ || cellCount_ == 0) return 0;
    return getCellMaxMilliVolts() - getCellMinMilliVolts();
}


// ---------------------------------------------------------------------------
void JbdBms::printFrameHex(const uint8_t* data, size_t len) {
    DEBUG_PRINT("[JBD] ");
    for (size_t i = 0; i < len; i++) {
        if (data[i] < 0x10) DEBUG_PRINT("0");
        DEBUG_PRINT_HEX(data[i], HEX);
        DEBUG_PRINT(" ");
    }
    DEBUG_PRINTLN();
}

// ---------------------------------------------------------------------------
// printBasicInfo
// ---------------------------------------------------------------------------
void JbdBms::printBasicInfo() {
    DEBUG_PRINTLN("[JBD] ===== Basic Info =====");
    DEBUG_PRINT("[JBD] Voltage: ");
    DEBUG_PRINT(packVoltageMilliVolts_ / 1000);
    DEBUG_PRINT(".");
    DEBUG_PRINT((packVoltageMilliVolts_ % 1000) / 10);  // 2 decimal places
    DEBUG_PRINTLN(" V");
    DEBUG_PRINT("[JBD] Current: ");
    DEBUG_PRINT(packCurrentMilliAmps_);
    DEBUG_PRINTLN(" mA");
    DEBUG_PRINT("[JBD] SoC: ");
    DEBUG_PRINT(socPercent_);
    DEBUG_PRINTLN(" %");
    DEBUG_PRINT("[JBD] Cells: ");
    DEBUG_PRINTLN(cellCount_);
    DEBUG_PRINT("[JBD] Cycles: ");
    DEBUG_PRINTLN(cycleCount_);
    DEBUG_PRINT("[JBD] Nominal cap: ");
    DEBUG_PRINT(designCapacityMahl_);
    DEBUG_PRINTLN(" mAh");
    DEBUG_PRINT("[JBD] Remaining cap: ");
    DEBUG_PRINT(balanceCapacityMahl_);
    DEBUG_PRINTLN(" mAh");
    DEBUG_PRINT("[JBD] FET CHG=");
    DEBUG_PRINT(chgFetEnabled_ ? "ON" : "OFF");
    DEBUG_PRINT(" DSG=");
    DEBUG_PRINTLN(dsgFetEnabled_ ? "ON" : "OFF");
    if (currentErrors_) {
        DEBUG_PRINT("[JBD] ERRORS: 0x");
        DEBUG_PRINT_HEX(currentErrors_, HEX);
        DEBUG_PRINTLN();
    }
    for (uint8_t i = 0; i < ntcCount_ && i < JBD_MAX_NTC; i++) {
        DEBUG_PRINT("[JBD] NTC");
        DEBUG_PRINT(i);
        DEBUG_PRINT(": ");
        DEBUG_PRINT(ntcTempsCelsius_[i]);
        DEBUG_PRINTLN(" C");
    }
    DEBUG_PRINTLN("[JBD] =====================");
}

// ---------------------------------------------------------------------------
// getNtcTempCelsius
// ---------------------------------------------------------------------------
int16_t JbdBms::getNtcTempCelsius(uint8_t index) const {
    if (index >= JBD_MAX_NTC || index >= ntcCount_) return 0;
    return ntcTempsCelsius_[index];
}
