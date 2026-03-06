#include "JbdBms.h"
#include "../config.h"
#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEUtils.h>
#include <BLE2902.h>

static JbdBms* s_jbdBms = nullptr;

// ---------------------------------------------------------------------------
// Callback global de notify
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
      state_(Idle), connected_(false), hasData_(false),
      lastConnectAttempt_(0), lastRequestMillis_(0), rxLen_(0),
      packVoltageMilliVolts_(0), packCurrentMilliAmps_(0), socPercent_(0),
      cellCount_(0), ntcCount_(0), cycleCount_(0),
      designCapacityMahl_(0), balanceCapacityMahl_(0),
      chgFetEnabled_(0), dsgFetEnabled_(0), currentErrors_(0)
{
    memset(ntcTempsCelsius_, 0, sizeof(ntcTempsCelsius_));
    memset(rxBuffer_, 0, sizeof(rxBuffer_));
}

// ---------------------------------------------------------------------------
// init
// ---------------------------------------------------------------------------
void JbdBms::init() {
    s_jbdBms = this;
    // BLEDevice::init() já deve ter sido chamado pelo sistema principal
    pClient_ = BLEDevice::createClient();
    if (!pClient_) {
        DEBUG_PRINTLN("[JBD] ERRO: createClient falhou");
        return;
    }
    state_ = Idle;
    DEBUG_PRINTLN("[JBD] Init OK | addr: " JBD_BMS_BLE_ADDRESS);
}

// ---------------------------------------------------------------------------
// resetConnection — limpa estado BLE para nova tentativa
// ---------------------------------------------------------------------------
void JbdBms::resetConnection() {
    if (pClient_ && pClient_->isConnected()) {
        pClient_->disconnect();
    }
    connected_ = false;
    pCharRx_   = nullptr;
    pCharTx_   = nullptr;
    rxLen_     = 0;
    state_     = Idle;
}

// ---------------------------------------------------------------------------
// update — máquina de estados, chamar no loop()
// ---------------------------------------------------------------------------
void JbdBms::update() {
    switch (state_) {

        // ------------------------------------------------------------------
        case Idle:
            if (millis() - lastConnectAttempt_ >= CONNECT_RETRY_MS) {
                lastConnectAttempt_ = millis();
                DEBUG_PRINTLN("[JBD] Conectando...");
                state_ = Connecting;
            }
            break;

        // ------------------------------------------------------------------
        case Connecting: {
            BLEAddress addr(JBD_BMS_BLE_ADDRESS);
            if (pClient_->connect(addr)) {
                connected_ = true;
                rxLen_     = 0;
                DEBUG_PRINTLN("[JBD] Conectado");
                state_ = Connected;
            } else {
                DEBUG_PRINTLN("[JBD] Falha na conexão, tentando novamente...");
                resetConnection();
            }
            break;
        }

        // ------------------------------------------------------------------
        case Connected: {
            // Localiza o service JBD (FF00)
            BLERemoteService* pService = pClient_->getService(JBD_SERVICE_UUID);
            if (!pService) {
                DEBUG_PRINTLN("[JBD] Service FF00 não encontrado");
                resetConnection();
                break;
            }

            // Localiza características
            pCharRx_ = pService->getCharacteristic(JBD_CHAR_UUID_RX); // FF01 notify
            pCharTx_ = pService->getCharacteristic(JBD_CHAR_UUID_TX); // FF02 write

            if (!pCharRx_ || !pCharTx_) {
                DEBUG_PRINTLN("[JBD] Characteristics FF01/FF02 não encontradas");
                resetConnection();
                break;
            }

            // FIX: ESP32-C3 precisa escrever no CCCD descriptor manualmente.
            // registerForNotify por si só pode não ativar as notificações no C3.
            pCharRx_->registerForNotify(onNotifyCallback);

            // Escreve 0x0100 no descriptor 0x2902 para habilitar notify explicitamente
            BLERemoteDescriptor* pCccd = pCharRx_->getDescriptor(BLEUUID((uint16_t)0x2902));
            if (pCccd) {
                uint8_t notifyOn[2] = {0x01, 0x00};
                pCccd->writeValue(notifyOn, 2, true);
                DEBUG_PRINTLN("[JBD] CCCD escrito manualmente (0x0100)");
            } else {
                DEBUG_PRINTLN("[JBD] AVISO: descriptor 0x2902 não encontrado — notify pode não funcionar");
            }

            state_             = Subscribed;
            lastRequestMillis_ = 0; // força envio imediato
            DEBUG_PRINTLN("[JBD] Subscrito em FF02, pronto para leituras");
            break;
        }

        // ------------------------------------------------------------------
        case Subscribed:
            if (!pClient_->isConnected()) {
                DEBUG_PRINTLN("[JBD] Desconectado inesperadamente");
                resetConnection();
                break;
            }

            // Processa frames acumulados no buffer de RX
            processRxBuffer();

            // Envia requisição periódica
            if (millis() - lastRequestMillis_ >= REQUEST_INTERVAL_MS) {
                lastRequestMillis_ = millis();
                sendBasicInfoRequest();
            }
            break;
    }
}

// ---------------------------------------------------------------------------
// onNotify — chamado pela ISR-like do BLE stack
// ---------------------------------------------------------------------------
void JbdBms::onNotify(uint8_t* data, size_t len) {
    if (rxLen_ + len > JBD_RX_BUFFER_SIZE) {
        // Overflow: descarta buffer para evitar lixo acumulado
        DEBUG_PRINTLN("[JBD] RX buffer overflow, descartando");
        rxLen_ = 0;
    }
    memcpy(rxBuffer_ + rxLen_, data, len);
    rxLen_ += len;
}

// ---------------------------------------------------------------------------
// processRxBuffer — extrai e valida frames JBD do buffer acumulado
//
// Este BMS (SP14S004 V1.0) envia respostas com wrapper "FF AA" antes do frame DD:
//   FF AA | DD | REG | STATUS | LEN | DATA[LEN] | CHK_H | CHK_L | 77
//
// Como BLE limita 20 bytes/pacote, a resposta pode vir em múltiplos chunks.
// onNotify() acumula tudo em rxBuffer_ e aqui montamos o frame completo.
//
// Checksum (recepção): 0x10000 - (STATUS + LEN + DATA[...])
// ---------------------------------------------------------------------------
void JbdBms::processRxBuffer() {
    while (rxLen_ > 0) {

        // --- Descarta wrapper FF AA se presente no início ---
        if (rxLen_ >= 2 && rxBuffer_[0] == 0xFF && rxBuffer_[1] == 0xAA) {
            memmove(rxBuffer_, rxBuffer_ + 2, rxLen_ - 2);
            rxLen_ -= 2;
            continue;
        }

        // --- Busca byte de início 0xDD ---
        if (rxBuffer_[0] != JBD_FRAME_START) {
            size_t skip = 0;
            while (skip < rxLen_ && rxBuffer_[skip] != JBD_FRAME_START) skip++;
            if (skip >= rxLen_) { rxLen_ = 0; return; }
            memmove(rxBuffer_, rxBuffer_ + skip, rxLen_ - skip);
            rxLen_ -= skip;
            continue;
        }

        // --- Precisa de pelo menos 4 bytes para ler LEN ---
        if (rxLen_ < 4) return;

        uint8_t reg     = rxBuffer_[1];
        uint8_t status  = rxBuffer_[2];
        uint8_t dataLen = rxBuffer_[3];

        // Frame completo: DD(1) + REG(1) + STATUS(1) + LEN(1) + DATA(N) + CHK_H(1) + CHK_L(1) + 77(1)
        size_t frameLen = (size_t)dataLen + 7;

        // Aguarda chegada do frame inteiro (pode vir em múltiplos pacotes BLE)
        if (rxLen_ < frameLen) return;

        // --- Verifica byte de fim 0x77 ---
        if (rxBuffer_[frameLen - 1] != JBD_FRAME_END) {
            DEBUG_PRINTLN("[JBD] Frame end inválido, ressincronizando...");
            memmove(rxBuffer_, rxBuffer_ + 1, rxLen_ - 1);
            rxLen_ -= 1;
            continue;
        }

        // --- Log do frame bruto ---
        DEBUG_PRINT("[JBD] RX: ");
        printFrameHex(rxBuffer_, frameLen);

        // --- Valida checksum: 0x10000 - (STATUS + LEN + DATA[...]) ---
        uint32_t sum = (uint32_t)status + dataLen;
        for (size_t i = 0; i < (size_t)dataLen; i++) sum += rxBuffer_[4 + i];
        uint16_t recvChk = ((uint16_t)rxBuffer_[4 + dataLen] << 8) | rxBuffer_[5 + dataLen];

        if (((sum + recvChk) & 0xFFFF) != 0) {
            DEBUG_PRINT("[JBD] Checksum inválido sum=0x");
            DEBUG_PRINT_HEX((uint16_t)(sum & 0xFFFF), HEX);
            DEBUG_PRINT(" chk=0x");
            DEBUG_PRINT_HEX(recvChk, HEX);
            DEBUG_PRINTLN();
            memmove(rxBuffer_, rxBuffer_ + 1, rxLen_ - 1);
            rxLen_ -= 1;
            continue;
        }

        // --- Frame válido: processa ---
        const uint8_t* dataPayload = rxBuffer_ + 4;

        if (status == 0x00) {
            if (reg == JBD_REG_BASIC_INFO) {
                parseBasicInfo(dataPayload, dataLen);
                printBasicInfo();
            }
        } else {
            DEBUG_PRINT("[JBD] BMS erro: reg=0x");
            DEBUG_PRINT_HEX(reg, HEX);
            DEBUG_PRINT(" status=0x");
            DEBUG_PRINT_HEX(status, HEX);
            DEBUG_PRINTLN();
        }

        // Consome frame do buffer
        memmove(rxBuffer_, rxBuffer_ + frameLen, rxLen_ - frameLen);
        rxLen_ -= frameLen;
    }
}

// ---------------------------------------------------------------------------
// buildReadFrame — monta frame de leitura JBD (7 bytes)
//
// Formato:  DD | A5 | REG | 00 | CHK_H | CHK_L | 77
// Checksum: 0x10000 - REG  (apenas o byte REG, sem CMD nem LEN)
//
// Confirmado pelo frame canônico: DD A5 03 00 FF FD 77
//   0x10000 - 0x03 = 0xFFFD ✓
// ---------------------------------------------------------------------------
void JbdBms::buildReadFrame(uint8_t reg, uint8_t* out, size_t* outLen) {
    uint16_t checksum = (uint16_t)(0x10000 - reg);  // só o REG

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
// buildWriteFrame — monta frame de escrita JBD
// Formato: DD | 5A | REG | LEN | DATA... | CHK_H | CHK_L | 77
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
// sendLoginRequest — envia senha padrão JBD (0x00000000) no reg 0x00
// Necessário em alguns firmwares antes de aceitar leituras
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
    if (!pCharTx_) return;
    uint8_t frame[8];
    size_t  frameLen;
    buildReadFrame(JBD_REG_BASIC_INFO, frame, &frameLen);
    DEBUG_PRINT("[JBD] TX: ");
    printFrameHex(frame, frameLen);
    pCharTx_->writeValue(frame, frameLen, false);
}

// ---------------------------------------------------------------------------
// parseBasicInfo — decodifica registro 0x03 (Basic Info)
//
// Protocolo JBD padrão, offset dentro do campo DATA:
//   0-1   Tensão total          (unidade: 10mV)
//   2-3   Corrente              (unidade: 10mA, signed)
//   4-5   Capacidade restante   (unidade: 10mAh)
//   6-7   Capacidade nominal    (unidade: 10mAh)
//   8-9   Ciclos de carga
//  10-11  Data de produção (BCD YYYYMMDD compactado)
//  12-13  Bitmask balance (células 1-16)
//  14-15  Bitmask balance (células 17-32)
//  16-17  Flags de proteção/erro
//    18   Versão de software
//    19   SoC (%)
//    20   FET status (bit0=CHG, bit1=DSG)
//    21   Nº de células em série
//    22   Nº de NTCs
//  23+    Temperatura NTC (2 bytes cada, Kelvin*10, big-endian)
// ---------------------------------------------------------------------------
void JbdBms::parseBasicInfo(const uint8_t* d, size_t dataLen) {
    if (dataLen < 23) {
        DEBUG_PRINT("[JBD] dataLen curto: ");
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
// printFrameHex
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
    DEBUG_PRINT("[JBD] Tensão: ");
    DEBUG_PRINT(packVoltageMilliVolts_ / 1000);
    DEBUG_PRINT(".");
    DEBUG_PRINT((packVoltageMilliVolts_ % 1000) / 10);  // 2 casas decimais
    DEBUG_PRINTLN(" V");
    DEBUG_PRINT("[JBD] Corrente: ");
    DEBUG_PRINT(packCurrentMilliAmps_);
    DEBUG_PRINTLN(" mA");
    DEBUG_PRINT("[JBD] SoC: ");
    DEBUG_PRINT(socPercent_);
    DEBUG_PRINTLN(" %");
    DEBUG_PRINT("[JBD] Células: ");
    DEBUG_PRINTLN(cellCount_);
    DEBUG_PRINT("[JBD] Ciclos: ");
    DEBUG_PRINTLN(cycleCount_);
    DEBUG_PRINT("[JBD] Cap. nominal: ");
    DEBUG_PRINT(designCapacityMahl_);
    DEBUG_PRINTLN(" mAh");
    DEBUG_PRINT("[JBD] Cap. restante: ");
    DEBUG_PRINT(balanceCapacityMahl_);
    DEBUG_PRINTLN(" mAh");
    DEBUG_PRINT("[JBD] FET CHG=");
    DEBUG_PRINT(chgFetEnabled_ ? "ON" : "OFF");
    DEBUG_PRINT(" DSG=");
    DEBUG_PRINTLN(dsgFetEnabled_ ? "ON" : "OFF");
    if (currentErrors_) {
        DEBUG_PRINT("[JBD] ERROS: 0x");
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
