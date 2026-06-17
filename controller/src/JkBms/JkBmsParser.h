#ifndef JK_BMS_PARSER_H
#define JK_BMS_PARSER_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

// ── Limits ────────────────────────────────────────────────────────────────────

#define JK_MAX_CELLS 32
#define JK_MAX_TEMPS  5

// Plausible per-cell voltage band (mV) — used to find the end of the cell array
// and to keep the cell-voltage sum (offset auto-detection) free of trailing fields.
#define JK_CELL_MIN_MV 1000
#define JK_CELL_MAX_MV 4500

// ── Frame constants ───────────────────────────────────────────────────────────

// Response sync header: 55 AA EB 90
#define JK_FRAME_TYPE_CELL_INFO   0x02
#define JK_FRAME_TYPE_DEVICE_INFO 0x03

// Command bytes written to characteristic FFE1
#define JK_CMD_CELL_INFO   0x96
#define JK_CMD_DEVICE_INFO 0x97

// JK02 BLE frames are a FIXED 300 bytes (both JK02_24S and JK02_32S). There is
// NO length field — the device always streams 300-byte records terminated by a
// 1-byte checksum at frame[299].
#define JK_FRAME_LENGTH 300

// ── Field offsets ───────────────────────────────────────────────────────────
//
// Response frame layout (JK02, header 55 AA EB 90):
//   frame[0..3]  = 55 AA EB 90 (sync)
//   frame[4]     = frame_type   (0x02 cell info, 0x03 device info)
//   frame[5]     = counter
//   frame[6..]   = data          ← cell voltages start HERE (no length field)
//   frame[299]   = checksum       (sum of frame[0..298] mod 256)
//
// ALL offsets below are relative to frame[0]. For JK02_32S (hardware >= v11)
// every field AFTER the cell array is shifted by +16 (the protocol offset);
// the cell array itself always starts at offset 6.
//
//   JK02_24S (hardware < v11):  24 cells, protocol offset = 0
//   JK02_32S (hardware >= v11): 32 cells, protocol offset = 16
//
// Source: syssi/esphome-jk-bms  jk_bms_ble.cpp  decode_jk02_cell_info_()

#define JK_CELL_BASE_OFFSET    6    // frame[6 + i*2] for cell i (LE uint16, mV)
#define JK_PACK_VOLTAGE_OFFSET 118  // + protocol_offset (LE uint32, mV)
#define JK_PACK_CURRENT_OFFSET 126  // + protocol_offset (LE int32, mA; negative = discharge)
#define JK_TEMP1_OFFSET        130  // + protocol_offset (LE int16, raw/10 = °C)
#define JK_TEMP2_OFFSET        132  // + protocol_offset (LE int16, raw/10 = °C)
#define JK_SOC_OFFSET          141  // + protocol_offset (uint8, %)

#define JK_HW_VERSION_OFFSET   22   // device info frame: null-terminated ASCII

// ── Types ─────────────────────────────────────────────────────────────────────

enum JkProtocol {
    JkProtocol_24S = 0,  // hardware < v11
    JkProtocol_32S = 1   // hardware >= v11
};

struct JkBmsData {
    uint32_t packVoltageMilliVolts;
    int32_t  packCurrentMilliAmps;
    uint8_t  socPercent;
    uint8_t  cellCount;
    uint8_t  tempCount;
    uint16_t cellVoltagesMv[JK_MAX_CELLS];
    int16_t  tempsCelsius[JK_MAX_TEMPS];
    bool     valid;
};

// ── Low-level helpers ─────────────────────────────────────────────────────────

static inline uint8_t jkComputeChecksum(const uint8_t* data, size_t len) {
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) sum += data[i];
    return sum;
}

static inline uint16_t jkGet16LE(const uint8_t* d, size_t offset) {
    return (uint16_t)d[offset] | ((uint16_t)d[offset + 1] << 8);
}

static inline uint32_t jkGet32LE(const uint8_t* d, size_t offset) {
    return (uint32_t)d[offset]
         | ((uint32_t)d[offset + 1] << 8)
         | ((uint32_t)d[offset + 2] << 16)
         | ((uint32_t)d[offset + 3] << 24);
}

// ── Command building ──────────────────────────────────────────────────────────

// Build a 20-byte JK command frame.
// Format: AA 55 90 EB [cmd] [14 zeros] [checksum]
static inline void jkBuildCommand(uint8_t cmd, uint8_t out[20]) {
    out[0] = 0xAA; out[1] = 0x55; out[2] = 0x90; out[3] = 0xEB;
    out[4] = cmd;
    for (int i = 5; i < 19; i++) out[i] = 0x00;
    out[19] = jkComputeChecksum(out, 19);
}

// ── Protocol detection ────────────────────────────────────────────────────────

// Detect protocol from hardware version string (e.g. "11.0XW", "V18A", "8.0").
// Hardware major version >= 11 → JK02_32S; otherwise → JK02_24S.
// Null/empty → JK02_32S (safe default for newer hardware).
static inline JkProtocol jkDetectProtocol(const char* hwVersion) {
    if (!hwVersion || !hwVersion[0]) return JkProtocol_32S;
    const char* p = hwVersion;
    if (*p == 'V' || *p == 'v') p++;
    int major = 0;
    while (*p >= '0' && *p <= '9') {
        major = major * 10 + (*p - '0');
        p++;
    }
    return (major >= 11) ? JkProtocol_32S : JkProtocol_24S;
}

static inline uint8_t jkProtocolOffset(JkProtocol protocol) {
    return (protocol == JkProtocol_32S) ? 16 : 0;
}

static inline uint8_t jkMaxCells(JkProtocol protocol) {
    return (protocol == JkProtocol_32S) ? 32 : 24;
}

// ── Frame validation ──────────────────────────────────────────────────────────

static inline bool jkHasResponseHeader(const uint8_t* buf, size_t len) {
    if (len < 4) return false;
    return buf[0] == 0x55 && buf[1] == 0xAA && buf[2] == 0xEB && buf[3] == 0x90;
}

// Checksum is the last byte of the frame: sum of all preceding bytes mod 256.
static inline bool jkValidateChecksum(const uint8_t* buf, size_t frameSize) {
    if (frameSize < 2) return false;
    return jkComputeChecksum(buf, frameSize - 1) == buf[frameSize - 1];
}

// ── Cell info parsing ─────────────────────────────────────────────────────────

// Parse a complete, checksum-validated 0x02 (cell info) frame into JkBmsData.
// All field offsets are relative to the frame start (buf[0]). Returns false if
// the buffer is shorter than a full JK02 frame.
static inline bool jkParseCellInfo(const uint8_t* buf, size_t frameSize,
                                    JkProtocol protocol, JkBmsData* out) {
    if (frameSize < JK_FRAME_LENGTH) return false;

    // Cell voltages: buf[JK_CELL_BASE_OFFSET + i*2], LE uint16, mV. The cell
    // array always starts at offset 6 (no protocol offset). Scan until a value
    // leaves the plausible per-cell band — that marks the end of the array.
    uint8_t  cellCount = 0;
    uint32_t cellSum   = 0;
    for (uint8_t i = 0; i < JK_MAX_CELLS; i++) {
        size_t pos = JK_CELL_BASE_OFFSET + (size_t)i * 2;
        if (pos + 1 >= frameSize) break;
        uint16_t mv = jkGet16LE(buf, pos);
        if (mv < JK_CELL_MIN_MV || mv > JK_CELL_MAX_MV) break;
        out->cellVoltagesMv[i] = mv;
        cellSum += mv;
        cellCount++;
    }
    out->cellCount = cellCount;

    // Auto-detect the 24S/32S field offset. Every post-cell field shifts by +16
    // for 32S, and the controller cannot reliably learn the variant from device
    // info. The pack-voltage field equals the sum of the series cells (a
    // physical invariant), so pick the candidate position (24S: 118, 32S: 134)
    // closest to that sum; fall back to the hint only when there are no cells.
    const uint32_t packV0  = jkGet32LE(buf, JK_PACK_VOLTAGE_OFFSET);
    const uint32_t packV16 = jkGet32LE(buf, JK_PACK_VOLTAGE_OFFSET + 16);
    uint8_t offset;
    if (cellSum == 0) {
        offset = jkProtocolOffset(protocol);
    } else {
        const uint32_t d0  = (packV0  > cellSum) ? packV0  - cellSum : cellSum - packV0;
        const uint32_t d16 = (packV16 > cellSum) ? packV16 - cellSum : cellSum - packV16;
        offset = (d0 <= d16) ? 0 : 16;
    }

    // Pack voltage: LE uint32, mV
    out->packVoltageMilliVolts = (offset == 0) ? packV0 : packV16;

    // Pack current: LE int32, mA (negative = discharge)
    out->packCurrentMilliAmps = (int32_t)jkGet32LE(buf, JK_PACK_CURRENT_OFFSET + offset);

    // Temperatures: LE int16, raw * 0.1 = °C → raw / 10 as integer
    int16_t raw1 = (int16_t)jkGet16LE(buf, JK_TEMP1_OFFSET + offset);
    int16_t raw2 = (int16_t)jkGet16LE(buf, JK_TEMP2_OFFSET + offset);
    out->tempsCelsius[0] = raw1 / 10;
    out->tempsCelsius[1] = raw2 / 10;
    out->tempCount = 2;

    // SoC: uint8, %
    out->socPercent = buf[JK_SOC_OFFSET + offset];

    out->valid = true;
    return true;
}

// ── Device info parsing ───────────────────────────────────────────────────────

// Parse the hardware version string from a complete 0x03 (device info) frame.
// The version is a null-terminated ASCII field at frame[JK_HW_VERSION_OFFSET].
// Writes up to hwVersionLen-1 chars into hwVersion (null-terminated).
static inline bool jkParseDeviceInfo(const uint8_t* buf, size_t frameSize,
                                      char* hwVersion, size_t hwVersionLen) {
    if (hwVersionLen == 0) return false;
    if (frameSize <= JK_HW_VERSION_OFFSET) return false;

    const char* src    = (const char*)(buf + JK_HW_VERSION_OFFSET);
    size_t      srcMax = frameSize - JK_HW_VERSION_OFFSET;
    size_t      i      = 0;
    while (i < hwVersionLen - 1 && i < srcMax && src[i] != '\0') {
        hwVersion[i] = src[i];
        i++;
    }
    hwVersion[i] = '\0';
    return i > 0;
}

#endif // JK_BMS_PARSER_H
