#include <iostream>
#include <cassert>
#include <cstring>
#include <stdint.h>
#include <stddef.h>
using namespace std;

#include "../src/JkBms/JkBmsParser.h"

// ── Real JK02 BLE frame model (per syssi/esphome-jk-bms jk_bms_ble.cpp) ────────
//
//   frame[0..3]  = 55 AA EB 90 (sync header)
//   frame[4]     = record type (0x02 cell info, 0x03 device info)
//   frame[5]     = frame counter
//   frame[6..]   = data — cell voltages start HERE (NO length field at 6-7)
//   frame[299]   = checksum = sum(frame[0..298]) & 0xFF
//
// Frame is a FIXED 300 bytes for both JK02_24S and JK02_32S. All documented
// field offsets are relative to frame[0]. For JK02_32S every post-cell-array
// field is shifted by +16 (the protocol offset).
static const size_t JK_TEST_FRAME_LEN = 300;

// ── Test helpers ──────────────────────────────────────────────────────────────

static void putLE16(uint8_t* buf, size_t off, uint16_t v) {
    buf[off]     = (uint8_t)(v & 0xFF);
    buf[off + 1] = (uint8_t)((v >> 8) & 0xFF);
}

static void putLE32(uint8_t* buf, size_t off, uint32_t v) {
    buf[off]     = (uint8_t)(v & 0xFF);
    buf[off + 1] = (uint8_t)((v >> 8) & 0xFF);
    buf[off + 2] = (uint8_t)((v >> 16) & 0xFF);
    buf[off + 3] = (uint8_t)((v >> 24) & 0xFF);
}

// Build a complete 300-byte cell-info response frame in the REAL layout:
// fields at frame-relative offsets, no length field, checksum at frame[299].
static size_t makeCellInfoFrame(uint8_t* buf, size_t bufSize,
                                 JkProtocol protocol,
                                 uint32_t packMv, int32_t currentMa, uint8_t soc,
                                 int16_t temp1Raw, int16_t temp2Raw,
                                 uint8_t cellCount, uint16_t cellMv) {
    if (JK_TEST_FRAME_LEN > bufSize) return 0;
    memset(buf, 0, JK_TEST_FRAME_LEN);

    buf[0] = 0x55; buf[1] = 0xAA; buf[2] = 0xEB; buf[3] = 0x90;
    buf[4] = JK_FRAME_TYPE_CELL_INFO;
    buf[5] = 0x01; // counter

    const uint8_t offset   = jkProtocolOffset(protocol);
    const uint8_t maxCells = jkMaxCells(protocol);

    // Cell voltages at frame[JK_CELL_BASE_OFFSET + i*2] (no protocol offset).
    uint8_t n = cellCount < maxCells ? cellCount : maxCells;
    for (uint8_t i = 0; i < n; i++) {
        putLE16(buf, JK_CELL_BASE_OFFSET + (size_t)i * 2, cellMv);
    }

    putLE32(buf, JK_PACK_VOLTAGE_OFFSET + offset, packMv);
    putLE32(buf, JK_PACK_CURRENT_OFFSET + offset, (uint32_t)currentMa);
    putLE16(buf, JK_TEMP1_OFFSET + offset, (uint16_t)temp1Raw);
    putLE16(buf, JK_TEMP2_OFFSET + offset, (uint16_t)temp2Raw);
    buf[JK_SOC_OFFSET + offset] = soc;

    buf[JK_TEST_FRAME_LEN - 1] = jkComputeChecksum(buf, JK_TEST_FRAME_LEN - 1);
    return JK_TEST_FRAME_LEN;
}

int main() {
    // ── Checksum ──────────────────────────────────────────────────────────────

    {
        uint8_t d[] = {0x01, 0x02, 0x03};
        assert(jkComputeChecksum(d, 3) == 0x06);
        cout << "PASS: checksum basic\n";
    }
    {
        uint8_t d[] = {0xFF, 0x01};
        assert(jkComputeChecksum(d, 2) == 0x00); // wraps at 256
        cout << "PASS: checksum overflow wraps\n";
    }

    // ── Command building ──────────────────────────────────────────────────────

    {
        uint8_t frame[20];
        jkBuildCommand(JK_CMD_DEVICE_INFO, frame);
        assert(frame[0] == 0xAA && frame[1] == 0x55 && frame[2] == 0x90 && frame[3] == 0xEB);
        assert(frame[4] == 0x97);
        for (int i = 5; i < 19; i++) assert(frame[i] == 0x00);
        // (0xAA+0x55+0x90+0xEB+0x97) & 0xFF = 785 & 0xFF = 17 = 0x11
        assert(frame[19] == 0x11);
        assert(frame[19] == jkComputeChecksum(frame, 19));
        cout << "PASS: device info command frame\n";
    }
    {
        uint8_t frame[20];
        jkBuildCommand(JK_CMD_CELL_INFO, frame);
        assert(frame[4] == 0x96);
        // (0xAA+0x55+0x90+0xEB+0x96) & 0xFF = 784 & 0xFF = 16 = 0x10
        assert(frame[19] == 0x10);
        cout << "PASS: cell info command frame\n";
    }

    // ── Protocol detection ────────────────────────────────────────────────────

    assert(jkDetectProtocol("11.0XW") == JkProtocol_32S);
    assert(jkDetectProtocol("15.XW")  == JkProtocol_32S);
    assert(jkDetectProtocol("V18A")   == JkProtocol_32S);
    assert(jkDetectProtocol("18.00")  == JkProtocol_32S);
    assert(jkDetectProtocol("v11.0")  == JkProtocol_32S);
    assert(jkDetectProtocol("8.0")    == JkProtocol_24S);
    assert(jkDetectProtocol("6.0")    == JkProtocol_24S);
    assert(jkDetectProtocol("10.9")   == JkProtocol_24S);
    assert(jkDetectProtocol("")       == JkProtocol_32S); // empty → safe default
    assert(jkDetectProtocol(nullptr)  == JkProtocol_32S); // null → safe default
    cout << "PASS: protocol detection\n";

    // ── Response header detection ─────────────────────────────────────────────

    {
        uint8_t good[] = {0x55, 0xAA, 0xEB, 0x90, 0x02};
        uint8_t bad[]  = {0x55, 0xAA, 0xEB, 0x91, 0x02};
        assert( jkHasResponseHeader(good, 5));
        assert(!jkHasResponseHeader(bad,  5));
        assert(!jkHasResponseHeader(good, 3)); // too short
        cout << "PASS: response header detection\n";
    }

    // ── Fixed frame length: checksum sits at frame[299] ───────────────────────

    {
        uint8_t buf[JK_TEST_FRAME_LEN] = {};
        buf[0] = 0x55; buf[1] = 0xAA; buf[2] = 0xEB; buf[3] = 0x90;
        buf[JK_TEST_FRAME_LEN - 1] = jkComputeChecksum(buf, JK_TEST_FRAME_LEN - 1);
        assert(jkValidateChecksum(buf, JK_TEST_FRAME_LEN));
        cout << "PASS: 300-byte frame, checksum at [299]\n";
    }

    // ── Real captured frame prefix (ground-truth anchor) ──────────────────────
    // Header + first cells from a real JK02 frame (syssi/esphome-jk-bms):
    //   55 AA EB 90 02 8C | FF 0C 01 0D 01 0D FF 0C ...
    // Cell 1 = 0x0CFF = 3327 mV, cell 2 = 0x0D01 = 3329 mV, etc. Cells live at
    // frame[6], little-endian, in millivolts — NO 8-byte payload shift.
    {
        uint8_t buf[JK_TEST_FRAME_LEN] = {};
        const uint8_t prefix[] = {
            0x55, 0xAA, 0xEB, 0x90, 0x02, 0x8C,
            0xFF, 0x0C, 0x01, 0x0D, 0x01, 0x0D, 0xFF, 0x0C
        };
        memcpy(buf, prefix, sizeof(prefix));
        buf[JK_TEST_FRAME_LEN - 1] = jkComputeChecksum(buf, JK_TEST_FRAME_LEN - 1);

        JkBmsData d = {};
        assert(jkParseCellInfo(buf, JK_TEST_FRAME_LEN, JkProtocol_24S, &d));
        assert(d.cellVoltagesMv[0] == 3327);
        assert(d.cellVoltagesMv[1] == 3329);
        assert(d.cellVoltagesMv[2] == 3329);
        assert(d.cellVoltagesMv[3] == 3327);
        assert(d.cellCount == 4); // cell 5 bytes are zero → stop
        cout << "PASS: real frame prefix — cells at frame[6], LE, mV\n";
    }

    // ── JK02_24S cell info parse ──────────────────────────────────────────────

    {
        uint8_t buf[JK_TEST_FRAME_LEN] = {};
        // 8 cells at 3500 mV, pack=44100 mV, I=-5000 mA (discharge), SoC=72%, T=25/28°C
        size_t sz = makeCellInfoFrame(buf, sizeof(buf),
                                      JkProtocol_24S,
                                      44100, -5000, 72,
                                      250, 280, // raw: 250/10=25°C, 280/10=28°C
                                      8, 3500);
        assert(sz == JK_TEST_FRAME_LEN);
        assert(jkHasResponseHeader(buf, sz));
        assert(jkValidateChecksum(buf, sz));

        JkBmsData d = {};
        assert(jkParseCellInfo(buf, sz, JkProtocol_24S, &d));
        assert(d.valid);
        assert(d.packVoltageMilliVolts == 44100);
        assert(d.packCurrentMilliAmps  == -5000);
        assert(d.socPercent == 72);
        assert(d.cellCount  == 8);
        for (uint8_t i = 0; i < 8; i++) assert(d.cellVoltagesMv[i] == 3500);
        assert(d.tempCount       == 2);
        assert(d.tempsCelsius[0] == 25);
        assert(d.tempsCelsius[1] == 28);
        cout << "PASS: JK02_24S cell info parse\n";
    }

    // ── JK02_32S cell info parse ──────────────────────────────────────────────

    {
        uint8_t buf[JK_TEST_FRAME_LEN] = {};
        // 20 cells at 3700 mV, pack=52000 mV, I=+10000 mA (charge), SoC=85%, T=30/31°C
        size_t sz = makeCellInfoFrame(buf, sizeof(buf),
                                      JkProtocol_32S,
                                      52000, 10000, 85,
                                      300, 310, // 30°C, 31°C
                                      20, 3700);
        assert(sz == JK_TEST_FRAME_LEN);
        assert(jkValidateChecksum(buf, sz));

        JkBmsData d = {};
        assert(jkParseCellInfo(buf, sz, JkProtocol_32S, &d));
        assert(d.valid);
        assert(d.packVoltageMilliVolts == 52000);
        assert(d.packCurrentMilliAmps  == 10000);
        assert(d.socPercent == 85);
        assert(d.cellCount  == 20);
        for (uint8_t i = 0; i < 20; i++) assert(d.cellVoltagesMv[i] == 3700);
        assert(d.tempsCelsius[0] == 30);
        assert(d.tempsCelsius[1] == 31);
        cout << "PASS: JK02_32S cell info parse\n";
    }

    // ── 24S and 32S use independent offsets (no cross-contamination) ──────────

    {
        uint8_t buf24[JK_TEST_FRAME_LEN] = {}, buf32[JK_TEST_FRAME_LEN] = {};
        makeCellInfoFrame(buf24, sizeof(buf24), JkProtocol_24S, 40000, 0, 60, 200, 200, 4, 3300);
        makeCellInfoFrame(buf32, sizeof(buf32), JkProtocol_32S, 50000, 0, 90, 200, 200, 4, 3300);

        JkBmsData d24 = {}, d32 = {};
        jkParseCellInfo(buf24, JK_TEST_FRAME_LEN, JkProtocol_24S, &d24);
        jkParseCellInfo(buf32, JK_TEST_FRAME_LEN, JkProtocol_32S, &d32);
        assert(d24.packVoltageMilliVolts == 40000 && d24.socPercent == 60);
        assert(d32.packVoltageMilliVolts == 50000 && d32.socPercent == 90);
        // Parsing a 32S frame with the 24S offset must NOT yield the 32S value.
        JkBmsData wrong = {};
        jkParseCellInfo(buf32, JK_TEST_FRAME_LEN, JkProtocol_24S, &wrong);
        assert(wrong.packVoltageMilliVolts != 50000);
        cout << "PASS: 24S/32S offset independence\n";
    }

    // ── Negative current (discharge) ─────────────────────────────────────────

    {
        uint8_t buf[JK_TEST_FRAME_LEN] = {};
        makeCellInfoFrame(buf, sizeof(buf), JkProtocol_32S,
                          48000, -12500, 45, 350, 360, 16, 3750);
        JkBmsData d = {};
        jkParseCellInfo(buf, JK_TEST_FRAME_LEN, JkProtocol_32S, &d);
        assert(d.packCurrentMilliAmps == -12500);
        cout << "PASS: negative current (discharge)\n";
    }

    // ── Corrupted frame rejected ──────────────────────────────────────────────

    {
        uint8_t buf[JK_TEST_FRAME_LEN] = {};
        makeCellInfoFrame(buf, sizeof(buf), JkProtocol_24S, 44100, 0, 50, 250, 250, 4, 3500);
        buf[JK_TEST_FRAME_LEN - 1] ^= 0xFF; // corrupt checksum byte
        assert(!jkValidateChecksum(buf, JK_TEST_FRAME_LEN));
        cout << "PASS: corrupted frame rejected\n";
    }

    // ── Frame too short to parse ──────────────────────────────────────────────

    {
        uint8_t tiny[8] = {0x55, 0xAA, 0xEB, 0x90, 0x02, 0x8C, 0x00, 0x00};
        JkBmsData d = {};
        assert(!jkParseCellInfo(tiny, 8, JkProtocol_24S, &d));
        assert(!d.valid);
        cout << "PASS: frame too short rejected\n";
    }

    // ── Device info parse (hardware version at frame[22]) ─────────────────────

    {
        uint8_t buf[JK_TEST_FRAME_LEN] = {};
        buf[0] = 0x55; buf[1] = 0xAA; buf[2] = 0xEB; buf[3] = 0x90;
        buf[4] = JK_FRAME_TYPE_DEVICE_INFO;
        buf[5] = 0x01;
        const char* hw = "11.0XW";
        memcpy(buf + JK_HW_VERSION_OFFSET, hw, strlen(hw) + 1);
        buf[JK_TEST_FRAME_LEN - 1] = jkComputeChecksum(buf, JK_TEST_FRAME_LEN - 1);

        assert(jkValidateChecksum(buf, JK_TEST_FRAME_LEN));
        char hwOut[16] = {};
        assert(jkParseDeviceInfo(buf, JK_TEST_FRAME_LEN, hwOut, sizeof(hwOut)));
        assert(strcmp(hwOut, "11.0XW") == 0);
        assert(jkDetectProtocol(hwOut) == JkProtocol_32S);
        cout << "PASS: device info parse + protocol detection from frame\n";
    }
    {
        // V18A string → JK02_32S
        uint8_t buf[JK_TEST_FRAME_LEN] = {};
        buf[0] = 0x55; buf[1] = 0xAA; buf[2] = 0xEB; buf[3] = 0x90;
        buf[4] = JK_FRAME_TYPE_DEVICE_INFO; buf[5] = 0x01;
        const char* hw = "V18A";
        memcpy(buf + JK_HW_VERSION_OFFSET, hw, strlen(hw) + 1);
        buf[JK_TEST_FRAME_LEN - 1] = jkComputeChecksum(buf, JK_TEST_FRAME_LEN - 1);
        char hwOut[16] = {};
        assert(jkParseDeviceInfo(buf, JK_TEST_FRAME_LEN, hwOut, sizeof(hwOut)));
        assert(strcmp(hwOut, "V18A") == 0);
        assert(jkDetectProtocol(hwOut) == JkProtocol_32S);
        cout << "PASS: V18A hardware version → JK02_32S\n";
    }

    cout << "\nAll JkBmsParser tests passed.\n";
    return 0;
}
