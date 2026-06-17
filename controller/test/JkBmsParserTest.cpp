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
// field is shifted by +16 (the protocol offset). esphome relies on the user to
// configure 24S vs 32S; this parser instead auto-detects the offset by matching
// the pack-voltage field against the sum of the cell voltages.
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

// Build a complete 300-byte cell-info response frame in the REAL layout, with
// the post-cell fields placed at the offset for `protocol`. The pack voltage is
// the sum of the cells (the physical invariant the parser uses to auto-detect).
static size_t makeCellInfoFrame(uint8_t* buf, size_t bufSize,
                                 JkProtocol protocol,
                                 int32_t currentMa, uint8_t soc,
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

    // Pack voltage = sum of cells (real packs satisfy this within measurement noise).
    const uint32_t packMv = (uint32_t)n * cellMv;
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
        assert(frame[19] == 0x11); // (0xAA+0x55+0x90+0xEB+0x97) & 0xFF
        assert(frame[19] == jkComputeChecksum(frame, 19));
        cout << "PASS: device info command frame\n";
    }
    {
        uint8_t frame[20];
        jkBuildCommand(JK_CMD_CELL_INFO, frame);
        assert(frame[4] == 0x96);
        assert(frame[19] == 0x10); // (0xAA+0x55+0x90+0xEB+0x96) & 0xFF
        cout << "PASS: cell info command frame\n";
    }

    // ── Protocol detection (hardware version string → variant hint) ───────────

    assert(jkDetectProtocol("11.0XW") == JkProtocol_32S);
    assert(jkDetectProtocol("V18A")   == JkProtocol_32S);
    assert(jkDetectProtocol("8.0")    == JkProtocol_24S);
    assert(jkDetectProtocol("10.9")   == JkProtocol_24S);
    assert(jkDetectProtocol("")       == JkProtocol_32S); // empty → safe default
    assert(jkDetectProtocol(nullptr)  == JkProtocol_32S); // null → safe default
    cout << "PASS: protocol detection\n";

    // ── Response header + checksum position ───────────────────────────────────

    {
        uint8_t good[] = {0x55, 0xAA, 0xEB, 0x90, 0x02};
        uint8_t bad[]  = {0x55, 0xAA, 0xEB, 0x91, 0x02};
        assert( jkHasResponseHeader(good, 5));
        assert(!jkHasResponseHeader(bad,  5));
        assert(!jkHasResponseHeader(good, 3));
        cout << "PASS: response header detection\n";
    }
    {
        uint8_t buf[JK_TEST_FRAME_LEN] = {};
        buf[0] = 0x55; buf[1] = 0xAA; buf[2] = 0xEB; buf[3] = 0x90;
        buf[JK_TEST_FRAME_LEN - 1] = jkComputeChecksum(buf, JK_TEST_FRAME_LEN - 1);
        assert(jkValidateChecksum(buf, JK_TEST_FRAME_LEN));
        cout << "PASS: 300-byte frame, checksum at [299]\n";
    }

    // ── Real captured frame prefix (ground-truth anchor) ──────────────────────
    // 55 AA EB 90 02 8C | FF 0C 01 0D 01 0D FF 0C ... — cells at frame[6], LE, mV.
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
        assert(d.cellCount == 4);
        cout << "PASS: real frame prefix — cells at frame[6], LE, mV\n";
    }

    // ── JK02_24S cell info parse ──────────────────────────────────────────────

    {
        uint8_t buf[JK_TEST_FRAME_LEN] = {};
        // 8 cells at 3500 mV → pack=28000 mV, I=-5000 mA (discharge), SoC=72%, T=25/28°C
        size_t sz = makeCellInfoFrame(buf, sizeof(buf), JkProtocol_24S,
                                      -5000, 72, 250, 280, 8, 3500);
        assert(sz == JK_TEST_FRAME_LEN);
        assert(jkValidateChecksum(buf, sz));

        JkBmsData d = {};
        assert(jkParseCellInfo(buf, sz, JkProtocol_24S, &d));
        assert(d.valid);
        assert(d.packVoltageMilliVolts == 28000); // 8 * 3500
        assert(d.packCurrentMilliAmps  == -5000);
        assert(d.socPercent == 72);
        assert(d.cellCount  == 8);
        for (uint8_t i = 0; i < 8; i++) assert(d.cellVoltagesMv[i] == 3500);
        assert(d.tempCount == 2);
        assert(d.tempsCelsius[0] == 25);
        assert(d.tempsCelsius[1] == 28);
        cout << "PASS: JK02_24S cell info parse\n";
    }

    // ── JK02_32S cell info parse ──────────────────────────────────────────────

    {
        uint8_t buf[JK_TEST_FRAME_LEN] = {};
        // 20 cells at 3700 mV → pack=74000 mV, I=+10000 mA (charge), SoC=85%, T=30/31°C
        size_t sz = makeCellInfoFrame(buf, sizeof(buf), JkProtocol_32S,
                                      10000, 85, 300, 310, 20, 3700);
        assert(jkValidateChecksum(buf, sz));

        JkBmsData d = {};
        assert(jkParseCellInfo(buf, sz, JkProtocol_32S, &d));
        assert(d.valid);
        assert(d.packVoltageMilliVolts == 74000); // 20 * 3700
        assert(d.packCurrentMilliAmps  == 10000);
        assert(d.socPercent == 85);
        assert(d.cellCount  == 20);
        for (uint8_t i = 0; i < 20; i++) assert(d.cellVoltagesMv[i] == 3700);
        assert(d.tempsCelsius[0] == 30);
        assert(d.tempsCelsius[1] == 31);
        cout << "PASS: JK02_32S cell info parse\n";
    }

    // ── Auto-detect offset from data, ignoring a WRONG protocol hint ──────────
    // This is the field bug: the controller defaulted to 32S but the device was
    // 24S, so every post-cell field read 16 bytes too far (V=0, I=huge, SoC=0).
    {
        uint8_t buf[JK_TEST_FRAME_LEN] = {};
        // A genuine 24S frame (fields at offset 0): 14 cells @ 3200 mV → pack=44800.
        makeCellInfoFrame(buf, sizeof(buf), JkProtocol_24S, -8000, 63, 240, 250, 14, 3200);

        JkBmsData d = {};
        // Parse with the WRONG (32S) hint — must still auto-detect 24S from data.
        assert(jkParseCellInfo(buf, JK_TEST_FRAME_LEN, JkProtocol_32S, &d));
        assert(d.cellCount == 14);
        assert(d.packVoltageMilliVolts == 44800); // 14 * 3200, NOT 0
        assert(d.packCurrentMilliAmps  == -8000);  // NOT a garbage huge value
        assert(d.socPercent == 63);
        assert(d.tempsCelsius[0] == 24);
        cout << "PASS: 24S frame auto-detected despite 32S hint\n";
    }
    {
        uint8_t buf[JK_TEST_FRAME_LEN] = {};
        // A genuine 32S frame (fields at offset 16): 16 cells @ 3600 → pack=57600.
        makeCellInfoFrame(buf, sizeof(buf), JkProtocol_32S, 5000, 77, 200, 210, 16, 3600);

        JkBmsData d = {};
        // Parse with the WRONG (24S) hint — must still auto-detect 32S from data.
        assert(jkParseCellInfo(buf, JK_TEST_FRAME_LEN, JkProtocol_24S, &d));
        assert(d.cellCount == 16);
        assert(d.packVoltageMilliVolts == 57600); // 16 * 3600
        assert(d.packCurrentMilliAmps  == 5000);
        assert(d.socPercent == 77);
        cout << "PASS: 32S frame auto-detected despite 24S hint\n";
    }

    // ── Negative current (discharge) ─────────────────────────────────────────

    {
        uint8_t buf[JK_TEST_FRAME_LEN] = {};
        makeCellInfoFrame(buf, sizeof(buf), JkProtocol_32S, -12500, 45, 350, 360, 16, 3750);
        JkBmsData d = {};
        jkParseCellInfo(buf, JK_TEST_FRAME_LEN, JkProtocol_32S, &d);
        assert(d.packCurrentMilliAmps == -12500);
        cout << "PASS: negative current (discharge)\n";
    }

    // ── Corrupted frame rejected ──────────────────────────────────────────────

    {
        uint8_t buf[JK_TEST_FRAME_LEN] = {};
        makeCellInfoFrame(buf, sizeof(buf), JkProtocol_24S, 0, 50, 250, 250, 4, 3500);
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

        char hwOut[16] = {};
        assert(jkParseDeviceInfo(buf, JK_TEST_FRAME_LEN, hwOut, sizeof(hwOut)));
        assert(strcmp(hwOut, "11.0XW") == 0);
        assert(jkDetectProtocol(hwOut) == JkProtocol_32S);
        cout << "PASS: device info parse + protocol detection from frame\n";
    }

    // ── Real captured frame (golden) — base offset 150 (neither 24S nor 32S) ──
    // From a user's JK BMS: 14 cells, pack ~44.49 V, temps ~20/21 C, SoC 6%.
    // Pack voltage sits at frame[150] (offset 32), so fixed 118/134 offsets read
    // zero — the cell-sum scan must find it.
    {
        static const uint8_t F[300] = {
            0x55,0xAA,0xEB,0x90,0x02,0x8C,0xA6,0x0C,0x7A,0x0C,0x86,0x0C,0x6B,0x0C,0x92,0x0C,
            0x68,0x0C,0x1A,0x0C,0x52,0x0C,0x4A,0x0C,0x27,0x0C,0x82,0x0C,0x9B,0x0C,0xA2,0x0C,
            0x1E,0x0C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
            0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
            0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x3F,0x00,0x00,0x6A,0x0C,0x8C,0x00,0x00,0x06,
            0xA6,0x00,0xAF,0x00,0xAC,0x00,0xA9,0x00,0xA5,0x00,0xA6,0x00,0xA5,0x00,0xA6,0x00,
            0x05,0x01,0xFD,0x00,0xA9,0x00,0xA9,0x00,0xAB,0x00,0xAB,0x00,0x00,0x00,0x00,0x00,
            0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
            0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
            0xCC,0x00,0x00,0x00,0x00,0x00,0xC6,0xAD,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
            0x00,0x00,0xCA,0x00,0xD3,0x00,0x00,0x00,0x08,0x00,0x00,0x00,0x00,0x06,0x29,0x04,
            0x00,0x00,0x50,0x46,0x00,0x00,0x1F,0x00,0x00,0x00,0x53,0xA5,0x08,0x00,0x64,0x00,
            0x00,0x00,0xED,0xA4,0x0E,0x00,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
            0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x00,0x01,0x00,0x00,0x00,0x05,0x04,0x00,0x00,
            0x00,0x00,0x26,0x72,0x3F,0x40,0x00,0x00,0x00,0x00,0x60,0x11,0x00,0x00,0x00,0x00,
            0x00,0x01,0x01,0x06,0x00,0x00,0xBC,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0xCC,0x00,
            0x00,0x00,0x00,0x00,0x05,0x04,0x5F,0x00,0x00,0x00,0x16,0x06,0x00,0x00,0x80,0x51,
            0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFE,
            0xFF,0x7F,0xDC,0x0F,0x01,0x00,0x80,0x03,0x00,0x00,0x00,0xDC
        };
        assert(jkValidateChecksum(F, sizeof(F)));   // checksum at [299]
        JkBmsData d = {};
        // The protocol hint is irrelevant — the scan must find the real layout.
        assert(jkParseCellInfo(F, sizeof(F), JkProtocol_32S, &d));
        assert(d.cellCount == 14);
        assert(d.cellVoltagesMv[0] == 3238);        // first cell (0x0CA6)
        assert(d.cellVoltagesMv[6] == 3098);        // lowest cell (0x0C1A)
        assert(d.packVoltageMilliVolts == 44486);   // frame[150], NOT 0
        assert(d.packCurrentMilliAmps  == 0);       // at rest
        assert(d.tempsCelsius[0] == 20);            // frame[162] -> 20.2 C
        assert(d.tempsCelsius[1] == 21);            // frame[164] -> 21.1 C
        assert(d.socPercent == 6);                  // frame[173]
        cout << "PASS: real captured frame (base offset 150) decoded correctly\n";
    }

    cout << "\nAll JkBmsParser tests passed.\n";
    return 0;
}
