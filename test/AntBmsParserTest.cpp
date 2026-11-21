// Host tests for src/AntBms/AntBmsParser.h.
// Fixtures: real captures from aiobmsble tests/bms/test_ant_bms.py.
// Build: g++ -std=c++17 -Wall test/AntBmsParserTest.cpp -o $TMPDIR/antt && $TMPDIR/antt
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../src/AntBms/AntBmsParser.h"

// aiobmsble status capture: 22 cells, 4 probes, 168 bytes.
static const char* STATUS_HEX =
    "7ea11100009e050404160000000000000000000088010000000000000000000000001e091b091e091d091d091e0920091e098f088e0890082209eb08200920091f091e091f091e091f0921091f091d001d001d001d001e001e00e01315000a0064000101000000b4c40486f197003aede8006a000000a1f2c0030000000022090c008e080a009400080900006d006a00af02f3fa4c74e5002866ec00c35d6200ccb39200edc8aa55";
// aiobmsble device-info capture: 48 bytes, 6 extra bytes after the CRC.
static const char* DEVINFO_HEX =
    "7ea1126c02203234424800000000000000000000000032344248554230302d323131303236415796ff0b000041f2aa55";

static const size_t CELL_OFFSET = 34;

static std::vector<uint8_t> hexToBytes(const char* hex) {
    std::vector<uint8_t> out;
    for (size_t i = 0; hex[i] && hex[i + 1]; i += 2) {
        unsigned v = 0;
        sscanf(hex + i, "%2x", &v);
        out.push_back((uint8_t)v);
    }
    return out;
}

static AntFeedResult feedAll(AntFrameAssembler& a, const std::vector<uint8_t>& b, size_t chunk) {
    AntFeedResult r = AntFeedResult::None;
    for (size_t i = 0; i < b.size(); i += chunk) {
        size_t n = b.size() - i < chunk ? b.size() - i : chunk;
        r = a.feed(b.data() + i, n);
    }
    return r;
}

static void fixCrc(std::vector<uint8_t>& f) {
    size_t len = f[5];
    uint16_t c = antCrc16(f.data() + 1, 5 + len);
    f[6 + len] = c & 0xFF;
    f[7 + len] = c >> 8;
}

static void test_crc_matches_published_requests() {
    uint8_t out[10];
    assert(antBuildRequest(0x01, 0x0000, 0xBE, out, sizeof(out)) == 10);
    const uint8_t s[10] = {0x7E, 0xA1, 0x01, 0x00, 0x00, 0xBE, 0x18, 0x55, 0xAA, 0x55};
    assert(memcmp(out, s, 10) == 0);
    assert(antBuildRequest(0x02, 0x026C, 0x20, out, sizeof(out)) == 10);
    const uint8_t d[10] = {0x7E, 0xA1, 0x02, 0x6C, 0x02, 0x20, 0x58, 0xC4, 0xAA, 0x55};
    assert(memcmp(out, d, 10) == 0);
    assert(antBuildRequest(0x01, 0, 0xBE, out, 9) == 0);
}

static void test_name_matcher() {
    assert(antNameLooksLikeAnt("ANT-BLE24BHUB"));
    assert(antNameLooksLikeAnt("ANT@BLE22AAUB"));
    assert(antNameLooksLikeAnt("ANT~BLE24EKUB-GATA"));
    assert(!antNameLooksLikeAnt("JK-B2A24S"));
    assert(!antNameLooksLikeAnt("ANTBLE24"));
    assert(!antNameLooksLikeAnt("ANT-BL"));
    assert(!antNameLooksLikeAnt(""));
    assert(!antNameLooksLikeAnt(nullptr));
}

static void test_status_fixture_whole() {
    AntFrameAssembler a;
    auto f = hexToBytes(STATUS_HEX);
    assert(f.size() == 168);
    assert(a.feed(f.data(), f.size()) == AntFeedResult::Frame);
    assert(a.frameLen() == 168);
    assert(memcmp(a.frame(), f.data(), 168) == 0);
}

static void test_status_fixture_20_byte_chunks() {
    AntFrameAssembler a;
    auto f = hexToBytes(STATUS_HEX);
    for (size_t i = 0; i < f.size(); i += 20) {
        size_t n = f.size() - i < 20 ? f.size() - i : 20;
        AntFeedResult r = a.feed(f.data() + i, n);
        if (i + n < f.size()) assert(r == AntFeedResult::None);
        else assert(r == AntFeedResult::Frame);
    }
    assert(a.frameLen() == 168);
}

static void test_split_inside_tail_and_header() {
    auto f = hexToBytes(STATUS_HEX);
    size_t points[2] = {166, 1};
    for (size_t p : points) {
        AntFrameAssembler a;
        assert(a.feed(f.data(), p) == AntFeedResult::None);
        assert(a.feed(f.data() + p, f.size() - p) == AntFeedResult::Frame);
        assert(a.frameLen() == 168);
    }
}

static void test_payload_aa55_does_not_end_frame() {
    auto f = hexToBytes(STATUS_HEX);
    f[CELL_OFFSET] = 0xAA;
    f[CELL_OFFSET + 1] = 0x55;
    fixCrc(f);
    for (size_t chunk : {size_t(168), size_t(20), size_t(1)}) {
        AntFrameAssembler a;
        size_t frames = 0;
        for (size_t i = 0; i < f.size(); i += chunk) {
            size_t n = f.size() - i < chunk ? f.size() - i : chunk;
            AntFeedResult r = a.feed(f.data() + i, n);
            assert(r == AntFeedResult::None || r == AntFeedResult::Frame);
            if (r == AntFeedResult::Frame) { frames++; assert(a.frameLen() == 168); }
        }
        assert(frames == 1);
    }
    // A 7E A1 pair inside the payload must not restart the frame either.
    f[CELL_OFFSET] = 0x7E;
    f[CELL_OFFSET + 1] = 0xA1;
    fixCrc(f);
    AntFrameAssembler a;
    // Chunk boundary lands exactly on the embedded 7E A1.
    assert(a.feed(f.data(), CELL_OFFSET) == AntFeedResult::None);
    assert(a.feed(f.data() + CELL_OFFSET, f.size() - CELL_OFFSET) == AntFeedResult::Frame);
    assert(a.frameLen() == 168);
}

static void test_devinfo_extra_section_accepted() {
    AntFrameAssembler a;
    auto d = hexToBytes(DEVINFO_HEX);
    assert(d.size() == 48);
    assert(a.feed(d.data(), d.size()) == AntFeedResult::Frame);
    assert(a.frameLen() == 48);
    auto s = hexToBytes(STATUS_HEX);
    assert(a.feed(s.data(), s.size()) == AntFeedResult::Frame);
    assert(a.frameLen() == 168);
    // Same, in 20-byte chunks.
    AntFrameAssembler b;
    assert(feedAll(b, d, 20) == AntFeedResult::Frame);
    assert(b.frameLen() == 48);
    assert(feedAll(b, s, 20) == AntFeedResult::Frame);
    assert(b.frameLen() == 168);
}

static void test_crc_error() {
    AntFrameAssembler a;
    auto f = hexToBytes(STATUS_HEX);
    f[CELL_OFFSET + 4] ^= 0x01;
    assert(a.feed(f.data(), f.size()) == AntFeedResult::CrcError);
    assert(a.bufferLen() > 0);
    auto g = hexToBytes(STATUS_HEX);
    assert(a.feed(g.data(), g.size()) == AntFeedResult::Frame);
}

static void test_length_error() {
    AntFrameAssembler a;
    auto f = hexToBytes(STATUS_HEX);
    f[166] = 0x00;  // broken tail
    assert(a.feed(f.data(), f.size()) == AntFeedResult::LengthError);
    auto g = hexToBytes(STATUS_HEX);
    assert(a.feed(g.data(), g.size()) == AntFeedResult::Frame);
}

static void test_overflow() {
    AntFrameAssembler a;
    std::vector<uint8_t> junk = {0x7E, 0xA1, 0x11, 0x00, 0x00, 0xFF};
    junk.resize(300, 0x00);
    AntFeedResult r = AntFeedResult::None;
    bool sawOverflow = false;
    for (size_t i = 0; i < junk.size(); i += 20) {
        size_t n = junk.size() - i < 20 ? junk.size() - i : 20;
        r = a.feed(junk.data() + i, n);
        if (r == AntFeedResult::Overflow) sawOverflow = true;
    }
    assert(sawOverflow);
    auto f = hexToBytes(STATUS_HEX);
    assert(a.feed(f.data(), f.size()) == AntFeedResult::Frame);

    AntFrameAssembler b;
    assert(b.feed(junk.data(), junk.size()) == AntFeedResult::Overflow);
    assert(b.feed(f.data(), f.size()) == AntFeedResult::Frame);
}

int main() {
    test_crc_matches_published_requests();
    test_name_matcher();
    test_status_fixture_whole();
    test_status_fixture_20_byte_chunks();
    test_split_inside_tail_and_header();
    test_payload_aa55_does_not_end_frame();
    test_devinfo_extra_section_accepted();
    test_crc_error();
    test_length_error();
    test_overflow();
    printf("All AntBmsParser tests passed.\n");
    return 0;
}
