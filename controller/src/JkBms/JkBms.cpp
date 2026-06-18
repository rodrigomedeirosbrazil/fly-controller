#include "JkBms.h"
#include "../config.h"
#include "../Throttle/Throttle.h"
#include <cstring>
#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <esp_gap_ble_api.h>

struct BleFrame {
    uint8_t data[20];
    size_t  len;
};

static JkBms* s_jkBms = nullptr;

// ---------------------------------------------------------------------------
// Global notify callback
// ---------------------------------------------------------------------------
void onJkNotifyCallback(BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
    if (s_jkBms && pData && length > 0) {
        s_jkBms->onNotify(pData, length);
    }
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
JkBms::JkBms()
    : pClient_(nullptr), pChar_(nullptr),
      txQueue_(nullptr), txTaskHandle_(nullptr),
      connectQueue_(nullptr), connectTaskHandle_(nullptr), stateMutex_(nullptr),
      connectSessionId_(0),
      state_(Idle), initialized_(false), enabled_(false), connected_(false),
      lastConnectAttempt_(0), lastRequestMillis_(0), lastCellDataMillis_(0),
      deviceInfoRequested_(false), hasCellData_(false),
      rxLen_(0), protocol_(JkProtocol_32S)
{
    memset(hwVersion_, 0, sizeof(hwVersion_));
    memset(rxBuffer_, 0, sizeof(rxBuffer_));
    memset(&data_, 0, sizeof(data_));
}

// ---------------------------------------------------------------------------
// init
// ---------------------------------------------------------------------------
void JkBms::init() {
    if (initialized_) return;
    s_jkBms = this;
    stateMutex_ = xSemaphoreCreateMutex();
    connectQueue_ = xQueueCreate(1, sizeof(uint8_t));
    if (stateMutex_ == nullptr || connectQueue_ == nullptr) {
        DEBUG_PRINTLN("[JK] ERROR: mutex or connect queue create failed");
        return;
    }
    if (xTaskCreate(connectTask, "jk_conn", 4096, this, 1, &connectTaskHandle_) != pdPASS) {
        DEBUG_PRINTLN("[JK] ERROR: connect task create failed");
        return;
    }
    txQueue_ = xQueueCreate(4, sizeof(BleFrame));
    xTaskCreate(txTask, "jk_tx", 2048, this, 1, &txTaskHandle_);
    state_ = Idle;
    initialized_ = true;
    DEBUG_PRINTLN("[JK] Init OK");
}

void JkBms::setEnabled(bool enabled) {
    if (!initialized_) return;
    enabled_ = enabled;
    if (!enabled_) {
        resetConnection();
    }
}

void JkBms::setMacAddress(const String& macAddress) {
    macAddress_ = macAddress;
    macAddress_.trim();
}

// ---------------------------------------------------------------------------
// resetConnection
// ---------------------------------------------------------------------------
void JkBms::resetConnection() {
    if (stateMutex_ == nullptr) {
        return;
    }
    xSemaphoreTake(stateMutex_, portMAX_DELAY);
    connectSessionId_++;
    applyResetConnectionLocked();
    xSemaphoreGive(stateMutex_);
}

// ---------------------------------------------------------------------------
// applyResetConnectionLocked — caller must hold stateMutex_
// ---------------------------------------------------------------------------
void JkBms::applyResetConnectionLocked() {
    if (pClient_ && pClient_->isConnected()) {
        pClient_->disconnect();
    }
    if (txQueue_ != nullptr) {
        xQueueReset(txQueue_);
    }
    connected_           = false;
    pChar_               = nullptr;
    rxLen_               = 0;
    deviceInfoRequested_ = false;
    hasCellData_         = false;
    lastCellDataMillis_  = 0;
    memset(&data_, 0, sizeof(data_));
    state_               = Idle;
}

// ---------------------------------------------------------------------------
// connectTask — runs BLEClient::connect AND service discovery off the main loop
// ---------------------------------------------------------------------------
void JkBms::connectTask(void* arg) {
    JkBms* self = static_cast<JkBms*>(arg);
    uint8_t cmd = 0;
    for (;;) {
        if (xQueueReceive(self->connectQueue_, &cmd, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        char mac[18];
        mac[0] = '\0';
        uint32_t session = 0;
        bool okToConnect = false;

        xSemaphoreTake(self->stateMutex_, portMAX_DELAY);
        session = self->connectSessionId_;
        if (self->macAddress_.length() >= 17) {
            strncpy(mac, self->macAddress_.c_str(), 18);
            mac[17] = '\0';
        }
        okToConnect = self->enabled_ && !throttle.isArmed() && strlen(mac) == 17;
        xSemaphoreGive(self->stateMutex_);

        if (!okToConnect) {
            xSemaphoreTake(self->stateMutex_, portMAX_DELAY);
            if (session == self->connectSessionId_) {
                self->applyResetConnectionLocked();
                self->lastConnectAttempt_ = millis();
            }
            xSemaphoreGive(self->stateMutex_);
            continue;
        }

        // ----- Phase 1: BLE connect -----
        if (self->pClient_ == nullptr) {
            self->pClient_ = BLEDevice::createClient();
        }
        // JK BMS devices may use either PUBLIC or RANDOM address type depending
        // on firmware version. Try PUBLIC first (8 s), then RANDOM.
        BLEAddress addr(mac);
        bool connectedOk = false;
        if (self->pClient_ != nullptr) {
            connectedOk = self->pClient_->connect(addr, BLE_ADDR_TYPE_PUBLIC);
            if (!connectedOk) {
                connectedOk = self->pClient_->connect(addr, BLE_ADDR_TYPE_RANDOM);
            }
        }

        if (!connectedOk) {
            DEBUG_PRINTLN("[JK] Connection failed (both address types), retrying...");
            xSemaphoreTake(self->stateMutex_, portMAX_DELAY);
            if (session == self->connectSessionId_) {
                self->applyResetConnectionLocked();
                self->lastConnectAttempt_ = millis();
            }
            xSemaphoreGive(self->stateMutex_);
            continue;
        }

        // Re-validate before slow ATT operations
        xSemaphoreTake(self->stateMutex_, portMAX_DELAY);
        bool stillValid = (session == self->connectSessionId_)
            && self->enabled_ && !throttle.isArmed();
        xSemaphoreGive(self->stateMutex_);

        if (!stillValid) {
            if (self->pClient_->isConnected()) self->pClient_->disconnect();
            xSemaphoreTake(self->stateMutex_, portMAX_DELAY);
            if (session == self->connectSessionId_) {
                self->applyResetConnectionLocked();
                self->lastConnectAttempt_ = millis();
            }
            xSemaphoreGive(self->stateMutex_);
            continue;
        }

        // ----- Phase 2: ATT service discovery -----
        BLERemoteService* pService = self->pClient_->getService(JK_SERVICE_UUID);
        if (!pService) {
            DEBUG_PRINTLN("[JK] Service FFE0 not found");
            self->pClient_->disconnect();
            xSemaphoreTake(self->stateMutex_, portMAX_DELAY);
            if (session == self->connectSessionId_) {
                self->applyResetConnectionLocked();
                self->lastConnectAttempt_ = millis();
            }
            xSemaphoreGive(self->stateMutex_);
            continue;
        }

        BLERemoteCharacteristic* pChar = pService->getCharacteristic(JK_CHAR_UUID);
        if (!pChar) {
            DEBUG_PRINTLN("[JK] Characteristic FFE1 not found");
            self->pClient_->disconnect();
            xSemaphoreTake(self->stateMutex_, portMAX_DELAY);
            if (session == self->connectSessionId_) {
                self->applyResetConnectionLocked();
                self->lastConnectAttempt_ = millis();
            }
            xSemaphoreGive(self->stateMutex_);
            continue;
        }

        pChar->registerForNotify(onJkNotifyCallback);

        // ESP32-C3 needs CCCD written explicitly to enable notifications
        BLERemoteDescriptor* pCccd = pChar->getDescriptor(BLEUUID((uint16_t)0x2902));
        if (pCccd) {
            uint8_t notifyOn[2] = {0x01, 0x00};
            pCccd->writeValue(notifyOn, 2, true);
            DEBUG_PRINTLN("[JK] CCCD written manually (0x0100)");
        } else {
            DEBUG_PRINTLN("[JK] WARNING: descriptor 0x2902 not found — notify may not work");
        }

        // ----- Phase 3: commit shared state under mutex -----
        xSemaphoreTake(self->stateMutex_, portMAX_DELAY);
        bool sessionStillCurrent = (session == self->connectSessionId_)
            && self->enabled_ && !throttle.isArmed();
        if (!sessionStillCurrent) {
            xSemaphoreGive(self->stateMutex_);
            if (self->pClient_->isConnected()) self->pClient_->disconnect();
            xSemaphoreTake(self->stateMutex_, portMAX_DELAY);
            if (session == self->connectSessionId_) {
                self->applyResetConnectionLocked();
                self->lastConnectAttempt_ = millis();
            }
            xSemaphoreGive(self->stateMutex_);
            continue;
        }
        self->pChar_             = pChar;
        self->connected_         = true;
        self->rxLen_             = 0;
        self->lastRequestMillis_ = 0;
        self->deviceInfoRequested_ = false;
        self->state_             = Subscribed;
        xSemaphoreGive(self->stateMutex_);
        DEBUG_PRINTLN("[JK] Subscribed, ready for reads");
    }
}

// ---------------------------------------------------------------------------
// txTask — FreeRTOS task that performs BLE writes
// ---------------------------------------------------------------------------
void JkBms::txTask(void* arg) {
    JkBms* self = static_cast<JkBms*>(arg);
    BleFrame frame;
    for (;;) {
        if (xQueueReceive(self->txQueue_, &frame, portMAX_DELAY)) {
            if (self->pChar_ && self->pClient_ && self->pClient_->isConnected()) {
                self->pChar_->writeValue(frame.data, frame.len, false);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// update — state machine, call from loop()
// ---------------------------------------------------------------------------
void JkBms::update() {
    if (!initialized_) return;
    switch (state_) {

        case Idle: {
            if (!enabled_ || macAddress_.length() < 17) break;
            if (throttle.isArmed()) break;
            {
                const unsigned long now = millis();
                if (lastConnectAttempt_ != 0 && (now - lastConnectAttempt_) < CONNECT_RETRY_MS) break;
            }
            if (connectQueue_ == nullptr) break;
            {
                uint8_t q = 1;
                if (xQueueSend(connectQueue_, &q, 0) != pdTRUE) break;
            }
            DEBUG_PRINTLN("[JK] Connecting...");
            state_ = Connecting;
            break;
        }

        case Connecting: {
            if (!enabled_ || macAddress_.length() < 17 || throttle.isArmed()) {
                resetConnection();
            }
            break;
        }

        case Subscribed: {
            if (!pClient_->isConnected()) {
                DEBUG_PRINTLN("[JK] Unexpectedly disconnected");
                resetConnection();
                break;
            }

            processRxBuffer();

            // Send 0x97 device info once right after subscribing
            if (!deviceInfoRequested_) {
                deviceInfoRequested_ = true;
                sendCommand(JK_CMD_DEVICE_INFO);
                lastRequestMillis_ = millis();
                break;
            }

            // Kick the cell-info stream with 0x96, then let the BMS push frames
            // on its own (like the official app / esphome). Each 0x96 write makes
            // the JK beep, so once frames are streaming we stop requesting and
            // only re-kick if the stream stalls (no frame for CELL_STREAM_TIMEOUT_MS).
            {
                const unsigned long now = millis();
                const bool streamStalled = (lastCellDataMillis_ == 0)
                    || (now - lastCellDataMillis_ >= CELL_STREAM_TIMEOUT_MS);
                if (streamStalled && (now - lastRequestMillis_ >= REQUEST_INTERVAL_MS)) {
                    lastRequestMillis_ = now;
                    sendCommand(JK_CMD_CELL_INFO);
                }
            }
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// sendCommand
// ---------------------------------------------------------------------------
void JkBms::sendCommand(uint8_t cmd) {
    if (!txQueue_) return;
    BleFrame frame;
    jkBuildCommand(cmd, frame.data);
    frame.len = 20;
    DEBUG_PRINT("[JK] TX cmd=0x");
    DEBUG_PRINT_HEX(cmd, HEX);
    DEBUG_PRINTLN();
    xQueueSend(txQueue_, &frame, 0);
}

// ---------------------------------------------------------------------------
// onNotify — called by BLE stack notify callback
// ---------------------------------------------------------------------------
void JkBms::onNotify(uint8_t* data, size_t len) {
    xSemaphoreTake(stateMutex_, portMAX_DELAY);
    if (rxLen_ + len > JK_RX_BUFFER_SIZE) {
        DEBUG_PRINTLN("[JK] RX buffer overflow, discarding");
        rxLen_ = 0;
    }
    memcpy(rxBuffer_ + rxLen_, data, len);
    rxLen_ += len;
    xSemaphoreGive(stateMutex_);
}

// ---------------------------------------------------------------------------
// processRxBuffer — find 55 AA EB 90 frames, validate, dispatch
// ---------------------------------------------------------------------------
void JkBms::processRxBuffer() {
    xSemaphoreTake(stateMutex_, portMAX_DELAY);

    bool done = false;
    while (!done && rxLen_ > 0) {

        // Locate response header 55 AA EB 90
        if (!jkHasResponseHeader(rxBuffer_, rxLen_)) {
            size_t skip = 0;
            while (skip + 3 < rxLen_) {
                if (rxBuffer_[skip] == 0x55 && rxBuffer_[skip + 1] == 0xAA
                    && rxBuffer_[skip + 2] == 0xEB && rxBuffer_[skip + 3] == 0x90) {
                    break;
                }
                skip++;
            }
            if (skip + 3 >= rxLen_) {
                // No full header found; keep the trailing bytes (a header may be
                // split across the next notification) and wait for more data.
                if (rxLen_ > 3) {
                    memmove(rxBuffer_, rxBuffer_ + (rxLen_ - 3), 3);
                    rxLen_ = 3;
                }
                done = true;
                continue;
            }
            memmove(rxBuffer_, rxBuffer_ + skip, rxLen_ - skip);
            rxLen_ -= skip;
            continue;
        }

        // JK02 frames are a fixed 300 bytes — wait for the whole frame to arrive.
        if (rxLen_ < JK_FRAME_LENGTH) { done = true; continue; }

        // Validate checksum (sum of bytes 0..298 == byte 299)
        if (!jkValidateChecksum(rxBuffer_, JK_FRAME_LENGTH)) {
            DEBUG_PRINTLN("[JK] Invalid checksum, resyncing");
            memmove(rxBuffer_, rxBuffer_ + 1, rxLen_ - 1);
            rxLen_ -= 1;
            continue;
        }

        // Dispatch by frame type
        uint8_t frameType = rxBuffer_[4];

        if (frameType == JK_FRAME_TYPE_DEVICE_INFO) {
            char hwOut[16] = {};
            if (jkParseDeviceInfo(rxBuffer_, JK_FRAME_LENGTH, hwOut, sizeof(hwOut))) {
                strncpy(hwVersion_, hwOut, sizeof(hwVersion_) - 1);
                hwVersion_[sizeof(hwVersion_) - 1] = '\0';
                protocol_ = jkDetectProtocol(hwVersion_);
                DEBUG_PRINT("[JK] HW version: ");
                DEBUG_PRINT(hwVersion_);
                DEBUG_PRINT(" → protocol=");
                DEBUG_PRINTLN(protocol_ == JkProtocol_32S ? "32S" : "24S");
            }
        } else if (frameType == JK_FRAME_TYPE_CELL_INFO) {
#if DEBUG
            DEBUG_PRINT("[JK] RX cell info (");
            DEBUG_PRINT(JK_FRAME_LENGTH);
            DEBUG_PRINT(" bytes): ");
            printFrameHex(rxBuffer_, JK_FRAME_LENGTH);
#endif
            JkBmsData parsed = {};
            if (jkParseCellInfo(rxBuffer_, JK_FRAME_LENGTH, protocol_, &parsed)) {
                data_ = parsed;
                hasCellData_ = true;
                lastCellDataMillis_ = millis();
                DEBUG_PRINT("[JK] V=");
                DEBUG_PRINT(data_.packVoltageMilliVolts);
                DEBUG_PRINT("mV I=");
                DEBUG_PRINT(data_.packCurrentMilliAmps);
                DEBUG_PRINT("mA SoC=");
                DEBUG_PRINT(data_.socPercent);
                DEBUG_PRINT("% cells=");
                DEBUG_PRINTLN(data_.cellCount);
            }
        }

        // Consume frame
        memmove(rxBuffer_, rxBuffer_ + JK_FRAME_LENGTH, rxLen_ - JK_FRAME_LENGTH);
        rxLen_ -= JK_FRAME_LENGTH;
    }

    xSemaphoreGive(stateMutex_);
}

// ---------------------------------------------------------------------------
// Getters
// ---------------------------------------------------------------------------
int16_t JkBms::getTempCelsius(uint8_t index) const {
    if (index >= JK_MAX_TEMPS || index >= data_.tempCount) return 0;
    return data_.tempsCelsius[index];
}

uint16_t JkBms::getCellVoltageMilliVolts(uint8_t index) const {
    if (index >= JK_MAX_CELLS || index >= data_.cellCount) return 0;
    return data_.cellVoltagesMv[index];
}

uint16_t JkBms::getCellMinMilliVolts() const {
    if (!hasCellData_ || data_.cellCount == 0) return 0;
    uint16_t vmin = 0xFFFF;
    for (uint8_t i = 0; i < data_.cellCount && i < JK_MAX_CELLS; i++)
        if (data_.cellVoltagesMv[i] < vmin) vmin = data_.cellVoltagesMv[i];
    return vmin;
}

uint16_t JkBms::getCellMaxMilliVolts() const {
    if (!hasCellData_ || data_.cellCount == 0) return 0;
    uint16_t vmax = 0;
    for (uint8_t i = 0; i < data_.cellCount && i < JK_MAX_CELLS; i++)
        if (data_.cellVoltagesMv[i] > vmax) vmax = data_.cellVoltagesMv[i];
    return vmax;
}

uint16_t JkBms::getCellDeltaMilliVolts() const {
    if (!hasCellData_ || data_.cellCount == 0) return 0;
    return getCellMaxMilliVolts() - getCellMinMilliVolts();
}

// ---------------------------------------------------------------------------
// printFrameHex
// ---------------------------------------------------------------------------
void JkBms::printFrameHex(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (data[i] < 0x10) { DEBUG_PRINT("0"); }
        DEBUG_PRINT_HEX(data[i], HEX);
        DEBUG_PRINT(" ");
    }
    DEBUG_PRINTLN();
}
