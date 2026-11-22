#pragma once

// Pure parser pieces for the ANT / JNFN "7E A1" BMS protocol: CRC, request
// builder, name matcher, notification frame assembler and status decoder. No Arduino
// dependencies, so it is host-tested (test/AntBmsParserTest.cpp).

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define ANT_FUNC_STATUS 0x01
#define ANT_REPLY_STATUS 0x11
#define ANT_STATUS_REQ_ADDR 0x0000
#define ANT_STATUS_REQ_LEN 0xBE
#define ANT_MAX_FRAME 256

#define ANT_FRAME_HEADER_SIZE 6
#define ANT_FRAME_OVERHEAD 10  // header 6 + CRC 2 + tail 2
#define ANT_REQUEST_SIZE 10

// CRC16/Modbus: init 0xFFFF, poly 0xA001 (reflected).
inline uint16_t antCrc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++) {
            crc = (crc & 1) ? (uint16_t)((crc >> 1) ^ 0xA001) : (uint16_t)(crc >> 1);
        }
    }
    return crc;
}

// Builds a read request: 7E A1 func addrLo addrHi len crcLo crcHi AA 55.
// Returns the bytes written (10), or 0 if outMax is too small.
inline size_t antBuildRequest(uint8_t func, uint16_t addr, uint8_t len, uint8_t* out, size_t outMax) {
    if (out == nullptr || outMax < ANT_REQUEST_SIZE) return 0;
    out[0] = 0x7E;
    out[1] = 0xA1;
    out[2] = func;
    out[3] = (uint8_t)(addr & 0xFF);
    out[4] = (uint8_t)(addr >> 8);
    out[5] = len;
    uint16_t crc = antCrc16(out + 1, 5);
    out[6] = (uint8_t)(crc & 0xFF);
    out[7] = (uint8_t)(crc >> 8);
    out[8] = 0xAA;
    out[9] = 0x55;
    return ANT_REQUEST_SIZE;
}

// "ANT", any one character ('-', '@', '~', ...), then "BLE".
inline bool antNameLooksLikeAnt(const char* name) {
    if (name == nullptr) return false;
    if (name[0] != 'A' || name[1] != 'N' || name[2] != 'T') return false;
    if (name[3] == '\0') return false;
    return name[4] == 'B' && name[5] == 'L' && name[6] == 'E';
}

enum class AntFeedResult : uint8_t { None, Frame, CrcError, LengthError, Overflow };

class AntFrameAssembler {
public:
    AntFeedResult feed(const uint8_t* chunk, size_t len) {
        if (discard_) {
            bufLen_ = 0;
            discard_ = false;
        }
        frameLen_ = 0;
        if (chunk == nullptr) len = 0;

        // A chunk that starts a new frame restarts assembly, unless the
        // current frame is still short of its expected end (the pair is then
        // payload that happens to read 7E A1).
        if (len >= 2 && chunk[0] == 0x7E && chunk[1] == 0xA1) {
            if (!(startsWithHeader() && expectedEnd() == 0)) bufLen_ = 0;
        }

        if (bufLen_ + len > ANT_MAX_FRAME) {
            size_t room = ANT_MAX_FRAME - bufLen_;
            memcpy(buf_ + bufLen_, chunk, room);
            bufLen_ = ANT_MAX_FRAME;
            discard_ = true;
            return AntFeedResult::Overflow;
        }
        if (len > 0) {
            memcpy(buf_ + bufLen_, chunk, len);
            bufLen_ += len;
        }

        sync();
        if (bufLen_ < ANT_FRAME_HEADER_SIZE) return AntFeedResult::None;

        size_t end = expectedEnd();
        if (end == 0) return AntFeedResult::None;

        const uint8_t payloadLen = buf_[5];
        if (!(buf_[end - 2] == 0xAA && buf_[end - 1] == 0x55)) {
            bufLen_ = end;
            discard_ = true;
            return AntFeedResult::LengthError;
        }
        const size_t crcPos = ANT_FRAME_HEADER_SIZE + payloadLen;
        uint16_t got = (uint16_t)(buf_[crcPos] | (buf_[crcPos + 1] << 8));
        if (antCrc16(buf_ + 1, 5 + payloadLen) != got) {
            bufLen_ = end;
            discard_ = true;
            return AntFeedResult::CrcError;
        }

        memcpy(frame_, buf_, end);
        frameLen_ = end;
        memmove(buf_, buf_ + end, bufLen_ - end);
        bufLen_ -= end;
        return AntFeedResult::Frame;
    }

    const uint8_t* frame() const { return frame_; }
    size_t frameLen() const { return frameLen_; }
    size_t bufferLen() const { return bufLen_; }

    void reset() {
        bufLen_ = 0;
        frameLen_ = 0;
        discard_ = false;
    }

private:
    bool startsWithHeader() const { return bufLen_ >= 2 && buf_[0] == 0x7E && buf_[1] == 0xA1; }

    // Index just past the tail of the frame at the buffer start, or 0 while
    // the frame is still incomplete.
    size_t expectedEnd() const {
        if (bufLen_ < ANT_FRAME_HEADER_SIZE) return 0;
        const size_t payloadLen = buf_[5];
        const size_t need = ANT_FRAME_HEADER_SIZE + payloadLen + 4;
        return bufLen_ >= need ? need : 0;
    }

    // Drops bytes up to the next 7E A1, keeping a trailing lone 7E.
    void sync() {
        if (startsWithHeader()) return;
        size_t i = 0;
        while (i + 1 < bufLen_ && !(buf_[i] == 0x7E && buf_[i + 1] == 0xA1)) i++;
        if (i + 1 < bufLen_) {
            memmove(buf_, buf_ + i, bufLen_ - i);
            bufLen_ -= i;
        } else if (bufLen_ > 0 && buf_[bufLen_ - 1] == 0x7E) {
            buf_[0] = 0x7E;
            bufLen_ = 1;
        } else {
            bufLen_ = 0;
        }
    }

    uint8_t buf_[ANT_MAX_FRAME];
    uint8_t frame_[ANT_MAX_FRAME];
    size_t bufLen_ = 0;
    size_t frameLen_ = 0;
    bool discard_ = false;
};

#define ANT_MAX_CELLS 32
#define ANT_MAX_PROBES 8

#define ANT_STATUS_CELLS_OFFSET 34
// Bytes from B to the end of the frame's data, the shortest seen in a real
// capture: 74 for 22S (6 + len - B = 164 - 90); a 14S pack carries 98.
// Requiring this much rejects cell/probe counts that overrun the frame.
#define ANT_STATUS_TAIL_SIZE 74

struct AntBmsData {
    bool valid;
    uint8_t cellCount, probeCount;
    uint16_t cellMv[ANT_MAX_CELLS];
    int16_t probeC[ANT_MAX_PROBES];
    uint32_t packMv;
    int32_t packCurrentMa;    // firmware convention: positive = charge
    uint8_t socPercent;
    uint32_t nominalCapacityMah;
    uint16_t cellMinMv, cellMaxMv, cellDeltaMv;
};

inline uint16_t antReadU16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
inline uint32_t antReadU32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// Decodes a complete, CRC-checked status frame (func 0x11). Returns false and
// leaves *out untouched when the counts do not fit the frame length.
inline bool antDecodeStatus(const uint8_t* f, size_t len, AntBmsData* out) {
    if (f == nullptr || out == nullptr || len < ANT_FRAME_HEADER_SIZE) return false;
    if (f[2] != ANT_REPLY_STATUS) return false;
    const size_t cells = f[9];
    const size_t probes = f[8];
    if (cells > ANT_MAX_CELLS || probes > ANT_MAX_PROBES) return false;
    const size_t b = ANT_STATUS_CELLS_OFFSET + 2 * cells + 2 * (probes + 2);
    const size_t dataEnd = ANT_FRAME_HEADER_SIZE + (size_t)f[5];
    if (b + ANT_STATUS_TAIL_SIZE > dataEnd) return false;
    if (dataEnd > len) return false;

    AntBmsData d;
    memset(&d, 0, sizeof(d));
    d.cellCount = (uint8_t)cells;
    d.probeCount = (uint8_t)probes;

    uint16_t minMv = 0, maxMv = 0;
    for (size_t i = 0; i < cells; i++) {
        uint16_t mv = antReadU16(f + ANT_STATUS_CELLS_OFFSET + 2 * i);
        d.cellMv[i] = mv;
        if (mv == 0) continue;
        if (minMv == 0 || mv < minMv) minMv = mv;
        if (mv > maxMv) maxMv = mv;
    }
    d.cellMinMv = minMv;
    d.cellMaxMv = maxMv;
    d.cellDeltaMv = (uint16_t)(maxMv - minMv);

    const uint8_t* p = f + ANT_STATUS_CELLS_OFFSET + 2 * cells;
    // The MOS and balancer temperatures follow the probes; b skips them.
    for (size_t i = 0; i < probes; i++) d.probeC[i] = (int16_t)antReadU16(p + 2 * i);

    // From b: pack voltage (x10 mV), current (int16, 0.1 A, the BMS reports
    // discharge as positive, so it is negated), SoC, ..., capacity in uAh at +12.
    const uint8_t* t = f + b;
    d.packMv = (uint32_t)antReadU16(t) * 10;
    d.packCurrentMa = -(int32_t)(int16_t)antReadU16(t + 2) * 100;
    d.socPercent = (uint8_t)antReadU16(t + 4);
    d.nominalCapacityMah = antReadU32(t + 12) / 1000;
    d.valid = true;
    *out = d;
    return true;
}
