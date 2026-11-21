// Host tests for src/AntBms/AntDiagBuffer.h.
// Build: g++ -std=c++17 -Wall test/AntDiagBufferTest.cpp -o $TMPDIR/adt && $TMPDIR/adt
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

#include "../src/AntBms/AntDiagBuffer.h"

static std::string drain(AntDiagBuffer& b) {
    char out[ANT_DIAG_PENDING_CAP + 64];
    size_t n = b.takePending(out, sizeof(out));
    b.commitWritten(n);
    return std::string(out, n);
}

static void test_hex_format() {
    const uint8_t d[] = {0x7e, 0xa1, 0x00};
    char out[16];
    assert(antFormatHex(d, 3, out, sizeof(out)) == 6);
    assert(strcmp(out, "7ea100") == 0);
    // outMax 6 holds 2 bytes (4 chars) plus NUL; the third byte does not fit whole.
    assert(antFormatHex(d, 3, out, 6) == 4);
    assert(strcmp(out, "7ea1") == 0);
    assert(antFormatHex(d, 3, out, 1) == 0);
    assert(out[0] == '\0');
    assert(antFormatHex(d, 0, out, sizeof(out)) == 0);
}

static void test_line_format() {
    AntDiagBuffer b;
    b.begin(0);
    assert(b.addLine(1234, "status 7ea1"));
    assert(drain(b) == "1234 status 7ea1\n");
}

static void test_pending_overflow_counts_and_reports_drops() {
    AntDiagBuffer b;
    b.begin(0);
    std::string text(200, 'x');
    int accepted = 0;
    while (b.addLine(1, text.c_str())) accepted++;
    assert(accepted > 10);
    assert(b.dropped() >= 1);
    assert(!b.full());
    assert(!b.addLine(2, text.c_str()));
    uint32_t dropped = b.dropped();
    assert(dropped == 2);
    drain(b);
    assert(b.addLine(99, "after"));
    char expected[64];
    snprintf(expected, sizeof(expected), "99 # dropped %u lines\n99 after\n", (unsigned)dropped);
    assert(drain(b) == expected);
    assert(b.dropped() == 0);
    assert(b.addLine(100, "next"));
    assert(drain(b) == "100 next\n");
}

static void test_file_cap_writes_truncated_once() {
    AntDiagBuffer b;
    b.begin(0);
    std::string text(90, 'y');
    std::string all;
    bool sawReject = false;
    for (int i = 0; i < 2000 && !sawReject; i++) {
        if (!b.addLine(i, text.c_str())) {
            sawReject = true;
            break;
        }
        all += drain(b);
    }
    assert(sawReject);
    assert(b.full());
    all += drain(b);
    assert(all.size() <= ANT_DIAG_FILE_CAP);
    const char* tail = "# truncated\n";
    assert(all.size() >= 12 && all.compare(all.size() - 12, 12, tail) == 0);
    size_t first = all.find("# truncated");
    assert(all.find("# truncated", first + 1) == std::string::npos);
    assert(!b.addLine(5000, "late"));
    assert(drain(b).empty());
}

static void test_begin_with_full_file_rejects_all() {
    AntDiagBuffer b;
    b.begin(ANT_DIAG_FILE_CAP - 12);
    assert(b.full());
    assert(!b.addLine(1, "x"));
    assert(drain(b).empty());
    AntDiagBuffer c;
    c.begin(ANT_DIAG_FILE_CAP + 100);
    assert(c.full());
    AntDiagBuffer d;
    d.begin(ANT_DIAG_FILE_CAP - 13);
    assert(!d.full());
}

static void test_take_and_commit_roundtrip() {
    AntDiagBuffer b;
    b.begin(10);
    assert(b.addLine(1, "a"));
    assert(b.addLine(2, "bb"));
    char out[64];
    size_t n = b.takePending(out, sizeof(out));
    assert(std::string(out, n) == "1 a\n2 bb\n");
    // Taking does not consume; a partial commit leaves the rest.
    char again[64];
    assert(b.takePending(again, sizeof(again)) == n);
    b.commitWritten(4);
    n = b.takePending(out, sizeof(out));
    assert(std::string(out, n) == "2 bb\n");
    b.commitWritten(n);
    assert(b.takePending(out, sizeof(out)) == 0);
    // A small outMax returns a prefix only.
    assert(b.addLine(3, "ccc"));
    assert(b.takePending(out, 3) == 3);
}

static void test_long_hex_line_is_not_cut() {
    // A 168-byte status frame as hex is 336 chars; it must come out whole.
    uint8_t frame[168];
    for (size_t i = 0; i < sizeof(frame); i++) frame[i] = (uint8_t)(i * 7 + 1);
    char text[400];
    int p = snprintf(text, sizeof(text), "status raw ");
    size_t hexLen = antFormatHex(frame, sizeof(frame), text + p, sizeof(text) - p);
    assert(hexLen == 336);
    AntDiagBuffer b;
    b.begin(0);
    assert(b.addLine(123456, text));
    std::string expect = std::string("123456 ") + text + "\n";
    assert(drain(b) == expect);
    // The largest assembler buffer (256 bytes, 512 hex chars) fits too.
    uint8_t big[256];
    for (size_t i = 0; i < sizeof(big); i++) big[i] = (uint8_t)i;
    char text2[600];
    int q = snprintf(text2, sizeof(text2), "asm ");
    assert(antFormatHex(big, sizeof(big), text2 + q, sizeof(text2) - q) == 512);
    assert(b.addLine(1, text2));
    assert(drain(b) == std::string("1 ") + text2 + "\n");
}

int main() {
    test_hex_format();
    test_line_format();
    test_pending_overflow_counts_and_reports_drops();
    test_file_cap_writes_truncated_once();
    test_begin_with_full_file_rejects_all();
    test_take_and_commit_roundtrip();
    test_long_hex_line_is_not_cut();
    printf("AntDiagBufferTest: all passed\n");
    return 0;
}
