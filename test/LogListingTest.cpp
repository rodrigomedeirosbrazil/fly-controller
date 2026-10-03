#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include "../src/BleControl/ControlProtocol.h"
#include "../src/Logger/LogListing.h"
using namespace std;

static bool valid(const char* s) { return isValidLogName(s, strlen(s)); }

void test_validator_matches_the_app() {
    // Same rule as fly-app's isValidLogName (log_protocol.dart).
    assert(valid("20261002_003.csv"));
    assert(valid("00012.csv"));
    assert(valid("notes.txt"));
    assert(valid("a.csv"));
    // The contract (and fly-app) only require the suffix: the extension alone
    // is a valid 4-character name.
    assert(valid(".csv"));
    assert(valid(".txt"));
    assert(!valid("csv"));
    assert(valid("12345678901234567890.csv"));    // exactly 24
    assert(!valid("123456789012345678901.csv"));  // 25
    assert(!valid(""));
    assert(!valid("/20261002_003.csv"));          // no leading slash on the wire
    assert(!valid("dir/a.csv"));
    assert(!valid("..csv"));                       // contains ".."
    assert(!valid("a..b.csv"));
    assert(!valid("a b.csv"));                     // space is not allowed
    assert(!valid("a.CSV"));
    assert(!valid("a.bin"));
    const char withNul[] = { 'a', '\0', '.', 'c', 's', 'v' };
    assert(!isValidLogName(withNul, sizeof(withNul)));
    const char high[] = { 'a', (char) 0xC3, '.', 'c', 's', 'v' };
    assert(!isValidLogName(high, sizeof(high)));
}

void test_extension_check_is_suffix_only() {
    // Delete-all removes every .csv/.txt in the root, valid name or not.
    assert(hasLogExtension("a-very-long-name-from-somewhere-else.csv", 40));
    assert(hasLogExtension("x.txt", 5));
    assert(!hasLogExtension("x.bin", 5));
    assert(!hasLogExtension("csv", 3));
}

void test_name_compare_is_bytewise_then_length() {
    assert(compareLogNames("a.csv", 5, "b.csv", 5) < 0);
    assert(compareLogNames("b.csv", 5, "a.csv", 5) > 0);
    assert(compareLogNames("a.csv", 5, "a.csv", 5) == 0);
    assert(compareLogNames("a", 1, "a.csv", 5) < 0);          // prefix sorts first
    assert(compareLogNames("00012.csv", 9, "20261002_001.csv", 16) < 0);
}

void test_free_bytes_never_underflows() {
    assert(logFreeBytes(131072, 100000) == 31072);
    assert(logFreeBytes(131072, 131072) == 0);
    assert(logFreeBytes(131072, 200000) == 0);
}

// Helper: feed entries, encode, return length.
struct Entry { const char* name; uint32_t size; };

static size_t buildPage(LogPageBuilder& b, const char* cursor, const Entry* e, size_t n,
                        uint8_t* out, size_t limit) {
    b.reset(cursor, cursor ? strlen(cursor) : 0);
    for (size_t i = 0; i < n; i++) {
        b.offer(e[i].name, strlen(e[i].name), e[i].size);
    }
    return b.encode(out, limit, 4096, 131072);
}

static uint32_t u32At(const uint8_t* p) { uint32_t v; memcpy(&v, p, 4); return v; }
static uint16_t u16At(const uint8_t* p) { uint16_t v; memcpy(&v, p, 2); return v; }

void test_page_header_and_ascending_order_from_unsorted_input() {
    const Entry e[] = {
        { "20261002_003.csv", 300 },
        { "00012.csv",        100 },
        { "20261002_001.csv", 200 },
    };
    LogPageBuilder b;
    uint8_t out[CONTROL_MAX_PAYLOAD];
    const size_t n = buildPage(b, "", e, 3, out, 240);

    assert(u32At(out)     == 4096);     // usedBytes
    assert(u32At(out + 4) == 131072);   // totalBytes
    assert(u16At(out + 8) == 3);        // fileCount
    assert(out[10] == 0);               // no more
    assert(out[11] == 3);               // entries in this page

    size_t o = 12;
    assert(u32At(out + o) == 100 && out[o + 4] == 9 && memcmp(out + o + 5, "00012.csv", 9) == 0);
    o += 5 + 9;
    assert(u32At(out + o) == 200 && memcmp(out + o + 5, "20261002_001.csv", 16) == 0);
    o += 5 + 16;
    assert(u32At(out + o) == 300 && memcmp(out + o + 5, "20261002_003.csv", 16) == 0);
    o += 5 + 16;
    assert(n == o);
}

void test_zero_byte_invalid_and_foreign_files_are_hidden() {
    const Entry e[] = {
        { "20261002_004.csv", 0 },     // Logger's pre-created placeholder
        { "config.json",      50 },
        { "a b.csv",          50 },
        { "20261002_001.csv", 10 },
    };
    LogPageBuilder b;
    uint8_t out[CONTROL_MAX_PAYLOAD];
    buildPage(b, "", e, 4, out, 240);
    assert(u16At(out + 8) == 1);
    assert(out[11] == 1);
    assert(memcmp(out + 12 + 5, "20261002_001.csv", 16) == 0);
}

void test_cursor_is_strictly_greater_and_file_count_is_total() {
    const Entry e[] = {
        { "a.csv", 1 }, { "b.csv", 1 }, { "c.csv", 1 },
    };
    LogPageBuilder b;
    uint8_t out[CONTROL_MAX_PAYLOAD];
    buildPage(b, "b.csv", e, 3, out, 240);
    assert(u16At(out + 8) == 3);     // all listable files, not just the page
    assert(out[11] == 1);
    assert(memcmp(out + 12 + 5, "c.csv", 5) == 0);
    assert(out[10] == 0);

    // A cursor that no longer exists (deleted between pages) still works.
    buildPage(b, "bb.csv", e, 3, out, 240);
    assert(out[11] == 1 && memcmp(out + 12 + 5, "c.csv", 5) == 0);

    // Past the end: empty page, no more.
    buildPage(b, "z.csv", e, 3, out, 240);
    assert(out[11] == 0 && out[10] == 0);
}

static void fillLongNames(char names[][LOG_MAX_NAME_LEN + 1], Entry* e, size_t count) {
    for (size_t i = 0; i < count; i++) {
        // 24 chars: "2026100_" + 12 digits + ".csv"
        snprintf(names[i], LOG_MAX_NAME_LEN + 1, "2026100_%012u.csv", (unsigned) i);
        e[i].name = names[i];
        e[i].size = 1000;
    }
}

void test_page_fits_the_ios_limit_and_sets_more() {
    char names[10][LOG_MAX_NAME_LEN + 1];
    Entry e[10];
    fillLongNames(names, e, 10);
    LogPageBuilder b;
    uint8_t out[CONTROL_MAX_PAYLOAD];

    // 24-char entry = 29 bytes; (178 - 12) / 29 = 5.
    size_t n = buildPage(b, "", e, 10, out, 178);
    assert(n <= 178);
    assert(out[11] == 5);
    assert(out[10] == 1);

    // (240 - 12) / 29 = 7.
    n = buildPage(b, "", e, 10, out, 240);
    assert(n <= 240);
    assert(out[11] == 7);
    assert(out[10] == 1);

    // Paging with the last returned name reaches every file exactly once.
    char cursor[LOG_MAX_NAME_LEN + 1] = "";
    size_t seen = 0;
    for (int guard = 0; guard < 10; guard++) {
        buildPage(b, cursor, e, 10, out, 178);
        const uint8_t count = out[11];
        size_t o = 12;
        for (uint8_t i = 0; i < count; i++) {
            const uint8_t len = out[o + 4];
            memcpy(cursor, out + o + 5, len);
            cursor[len] = '\0';
            o += 5 + len;
            seen++;
        }
        if (out[10] == 0) break;
    }
    assert(seen == 10);
}

void test_page_always_carries_at_least_one_entry_when_one_exists() {
    char names[1][LOG_MAX_NAME_LEN + 1];
    Entry e[1];
    fillLongNames(names, e, 1);
    LogPageBuilder b;
    uint8_t out[CONTROL_MAX_PAYLOAD];
    // Smallest limit that holds header + one 24-char entry.
    assert(buildPage(b, "", e, 1, out, 12 + 29) == 41);
    assert(out[11] == 1);
    // Below that, encode refuses rather than send a page that can never advance.
    assert(buildPage(b, "", e, 1, out, 40) == 0);
}

void test_builder_keeps_the_smallest_names_beyond_its_capacity() {
    // More files than the kept array: the page must still start at the
    // smallest names, whatever order the directory returns them in.
    const size_t total = LOG_PAGE_KEEP + 10;
    char names[LOG_PAGE_KEEP + 10][LOG_MAX_NAME_LEN + 1];
    Entry e[LOG_PAGE_KEEP + 10];
    for (size_t i = 0; i < total; i++) {
        const size_t k = total - 1 - i;          // descending input
        snprintf(names[i], sizeof(names[i]), "%05u.csv", (unsigned) k);
        e[i].name = names[i];
        e[i].size = 1;
    }
    LogPageBuilder b;
    uint8_t out[CONTROL_MAX_PAYLOAD];
    buildPage(b, "", e, total, out, 240);
    assert(u16At(out + 8) == total);
    assert(memcmp(out + 12 + 5, "00000.csv", 9) == 0);
    assert(out[10] == 1);
}

void test_retention_picks_oldest_and_never_the_current_file() {
    LogRetentionPicker p;
    p.reset("00003.csv", 9);
    p.offer("20261002_001.csv", 16, 500);
    p.offer("00003.csv", 9, 500);            // current: never deleted
    p.offer("00002.csv", 9, 0);              // placeholder: frees nothing
    p.offer("config.json", 11, 500);         // not a log
    p.offer("00005.csv", 9, 500);
    assert(p.hasPick());
    assert(strcmp(p.pick(), "00005.csv") == 0);

    p.reset("", 0);
    p.offer("20261002_002.csv", 16, 1);
    p.offer("20261002_001.csv", 16, 1);
    assert(strcmp(p.pick(), "20261002_001.csv") == 0);

    p.reset("only.csv", 8);
    p.offer("only.csv", 8, 100);
    assert(!p.hasPick());
}

int main() {
    test_validator_matches_the_app();
    test_extension_check_is_suffix_only();
    test_name_compare_is_bytewise_then_length();
    test_free_bytes_never_underflows();
    test_page_header_and_ascending_order_from_unsorted_input();
    test_zero_byte_invalid_and_foreign_files_are_hidden();
    test_cursor_is_strictly_greater_and_file_count_is_total();
    test_page_fits_the_ios_limit_and_sets_more();
    test_page_always_carries_at_least_one_entry_when_one_exists();
    test_builder_keeps_the_smallest_names_beyond_its_capacity();
    test_retention_picks_oldest_and_never_the_current_file();
    cout << "LogListingTest: all passed" << endl;
    return 0;
}
