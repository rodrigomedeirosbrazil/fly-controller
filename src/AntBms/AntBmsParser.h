#pragma once

// Pure parser pieces for the ANT / JNFN "7E A1" BMS protocol: CRC, request
// builder, name matcher and notification frame assembler. No Arduino
// dependencies, so it is host-tested (test/AntBmsParserTest.cpp).

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define ANT_FUNC_STATUS 0x01
#define ANT_FUNC_DEVICE_INFO 0x02
#define ANT_REPLY_STATUS 0x11
#define ANT_REPLY_DEVICE_INFO 0x12
#define ANT_STATUS_REQ_ADDR 0x0000
#define ANT_STATUS_REQ_LEN 0xBE
#define ANT_DEVINFO_REQ_ADDR 0x026C
#define ANT_DEVINFO_REQ_LEN 0x20
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
        const bool devInfo = buf_[2] == ANT_REPLY_DEVICE_INFO;
        if (!devInfo && !(buf_[end - 2] == 0xAA && buf_[end - 1] == 0x55)) {
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
    const uint8_t* buffer() const { return buf_; }
    size_t bufferLen() const { return bufLen_; }

    void reset() {
        bufLen_ = 0;
        frameLen_ = 0;
        discard_ = false;
    }

private:
    bool startsWithHeader() const { return bufLen_ >= 2 && buf_[0] == 0x7E && buf_[1] == 0xA1; }

    // Index just past the tail of the frame at the buffer start, or 0 while
    // the frame is still incomplete. Device info (0x12) carries a wrong
    // length byte and extra bytes after the CRC: it ends at the first AA 55
    // at or after crcPos + 2.
    size_t expectedEnd() const {
        if (bufLen_ < ANT_FRAME_HEADER_SIZE) return 0;
        const size_t payloadLen = buf_[5];
        if (buf_[2] == ANT_REPLY_DEVICE_INFO) {
            for (size_t i = ANT_FRAME_HEADER_SIZE + payloadLen + 2; i + 1 < bufLen_; i++) {
                if (buf_[i] == 0xAA && buf_[i + 1] == 0x55) return i + 2;
            }
            return 0;
        }
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
