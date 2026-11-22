// Host tests for src/AntBms/AntBmsParser.h.
// Fixtures: a real 22S capture from aiobmsble tests/bms/test_ant_bms.py, and a
// real 14S capture from a JNFN pack in the field (2026-11-22).
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
// Field capture: 14 cells, 4 probes, 176 bytes, pack at rest, both MOS on.
static const char* STATUS_14S_HEX =
    "7ea1110000a60501040e0000000000000000000080010000000000000000000000004b104c104b104d104b104c1048104a1049104b1049104b104c104a101b001a001a001b001b001b00e0160000640064000101000180027503e8017503ed9c000000000000a5070700000000004d1004004810070005004b1000006f006c000a00f1faba3100001f080100ea0700000b0601000000000076030000000000000000ffff0000000000000000fd1baa55";

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

static void test_14s_fixture_back_to_back() {
    auto s14 = hexToBytes(STATUS_14S_HEX);
    auto s22 = hexToBytes(STATUS_HEX);
    assert(s14.size() == 176);
    AntFrameAssembler a;
    assert(a.feed(s14.data(), s14.size()) == AntFeedResult::Frame);
    assert(a.frameLen() == 176);
    assert(a.feed(s22.data(), s22.size()) == AntFeedResult::Frame);
    assert(a.frameLen() == 168);
    // Same, in 20-byte chunks.
    AntFrameAssembler b;
    assert(feedAll(b, s14, 20) == AntFeedResult::Frame);
    assert(b.frameLen() == 176);
    assert(memcmp(b.frame(), s14.data(), 176) == 0);
}

static void test_crc_error() {
    AntFrameAssembler a;
    auto f = hexToBytes(STATUS_HEX);
    f[CELL_OFFSET + 4] ^= 0x01;
    assert(a.feed(f.data(), f.size()) == AntFeedResult::CrcError);
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

// Offset of the first byte after the cells, probes, MOS and balancer temps
// for the status fixture (22 cells, 4 probes).
static size_t statusB(const std::vector<uint8_t>& f) {
    return 34 + 2 * (size_t)f[9] + 2 * ((size_t)f[8] + 2);
}

static void test_decode_status_fixture() {
    auto f = hexToBytes(STATUS_HEX);
    AntBmsData d;
    memset(&d, 0, sizeof(d));
    assert(antDecodeStatus(f.data(), f.size(), &d));
    assert(d.valid);
    assert(d.cellCount == 22);
    assert(d.probeCount == 4);
    assert(d.packMv == 50880);
    assert(d.packCurrentMa == -2100);
    assert(d.socPercent == 10);
    assert(d.nominalCapacityMah == 80000);
    for (int i = 0; i < 4; i++) assert(d.probeC[i] == 29);
    assert(d.cellMinMv == 2190);
    assert(d.cellMaxMv == 2338);
    assert(d.cellDeltaMv == 148);
    assert(d.cellMv[0] == 0x091e);
}

// Values match what the vendor app showed for the same pack: 58.56 V, 100 %,
// 58.0 Ah, cells 4.168-4.173 V, probes 26-27 C.
static void test_decode_14s_field_fixture() {
    auto f = hexToBytes(STATUS_14S_HEX);
    AntBmsData d;
    memset(&d, 0, sizeof(d));
    assert(antDecodeStatus(f.data(), f.size(), &d));
    assert(d.valid);
    assert(d.cellCount == 14);
    assert(d.probeCount == 4);
    assert(d.packMv == 58560);
    assert(d.packCurrentMa == 0);
    assert(d.socPercent == 100);
    assert(d.nominalCapacityMah == 58000);
    assert(d.cellMinMv == 4168);
    assert(d.cellMaxMv == 4173);
    assert(d.cellDeltaMv == 5);
    assert(d.cellMv[0] == 4171);
    assert(d.cellMv[13] == 4170);
    const int16_t probes[4] = {27, 26, 26, 27};
    for (int i = 0; i < 4; i++) assert(d.probeC[i] == probes[i]);
}

static void test_decode_rejects_cell_overrun() {
    auto f = hexToBytes(STATUS_HEX);
    f[9] = 32;
    AntBmsData d;
    memset(&d, 0, sizeof(d));
    assert(!antDecodeStatus(f.data(), f.size(), &d));
    assert(!d.valid);
}

static void test_decode_rejects_probe_overrun() {
    auto f = hexToBytes(STATUS_HEX);
    f[8] = 255;
    AntBmsData d;
    memset(&d, 0, sizeof(d));
    assert(!antDecodeStatus(f.data(), f.size(), &d));
    assert(!d.valid);
}

static void test_decode_rejects_wrong_func() {
    auto f = hexToBytes(STATUS_HEX);
    f[2] = 0x12;
    AntBmsData d;
    memset(&d, 0, sizeof(d));
    assert(!antDecodeStatus(f.data(), f.size(), &d));
}

static void test_decode_negative_current() {
    auto f = hexToBytes(STATUS_HEX);
    size_t b = statusB(f);
    f[b + 2] = 0xDD;
    f[b + 3] = 0xFF;  // raw -35
    AntBmsData d;
    memset(&d, 0, sizeof(d));
    assert(antDecodeStatus(f.data(), f.size(), &d));
    assert(d.packCurrentMa == 3500);
}

int main() {
    test_crc_matches_published_requests();
    test_name_matcher();
    test_status_fixture_whole();
    test_status_fixture_20_byte_chunks();
    test_split_inside_tail_and_header();
    test_payload_aa55_does_not_end_frame();
    test_14s_fixture_back_to_back();
    test_crc_error();
    test_length_error();
    test_overflow();
    test_decode_status_fixture();
    test_decode_14s_field_fixture();
    test_decode_rejects_cell_overrun();
    test_decode_rejects_probe_overrun();
    test_decode_rejects_wrong_func();
    test_decode_negative_current();
    printf("All AntBmsParser tests passed.\n");
    return 0;
}
