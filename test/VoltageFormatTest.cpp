#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include "../src/Telemetry/VoltageFormat.h"
using namespace std;

static void expect(uint16_t millivolts, const char* want) {
    char buf[16];
    formatMilliVolts(buf, sizeof(buf), millivolts);
    if (strcmp(buf, want) != 0) {
        cout << "  " << millivolts << " mV -> got \"" << buf
             << "\", want \"" << want << "\"" << endl;
        assert(false);
    }
}

void test_decimal_is_always_three_digits() {
    // The field is documented as V.mmm in docs/XCTOD-PROTOCOL.md, and
    // millivolts % 1000 spans 0-999. The old code padded a single zero and
    // only below 10, so anything in 1-99 was transmitted with the decimal
    // point in the wrong place: 50040 mV went out as "50.40" -- 50.4 V, a
    // +0.36 V error on a reading XCTrack shows the pilot in flight.
    expect(50000, "50.000");
    expect(50005, "50.005");
    expect(50040, "50.040");
    expect(50099, "50.099");
    expect(50100, "50.100");
    expect(50400, "50.400");
    expect(50999, "50.999");
}

void test_boundaries() {
    expect(0,     "0.000");
    expect(1,     "0.001");
    expect(999,   "0.999");
    expect(1000,  "1.000");
    expect(65535, "65.535");   // uint16 max, the widest this field can be
}

void test_every_millivolt_round_trips_to_the_same_number() {
    // Parsing the text back must recover the original reading exactly, for
    // every value the field can hold. This is the property the old format
    // broke, and it breaks silently -- the output is still a valid decimal.
    for (uint32_t mv = 0; mv <= 65535; mv++) {
        char buf[16];
        formatMilliVolts(buf, sizeof(buf), (uint16_t) mv);

        char* end = nullptr;
        const double parsed = strtod(buf, &end);
        assert(end != nullptr && *end == '\0');
        assert((uint32_t) (parsed * 1000.0 + 0.5) == mv);
    }
}

void test_truncation_is_safe() {
    // A buffer too small must still leave a NUL-terminated string rather
    // than running off the end.
    char small[4];
    formatMilliVolts(small, sizeof(small), 50040);
    assert(small[sizeof(small) - 1] == '\0');
}

int main() {
    test_decimal_is_always_three_digits();
    test_boundaries();
    test_every_millivolt_round_trips_to_the_same_number();
    test_truncation_is_safe();
    cout << "VoltageFormatTest: all passed" << endl;
    return 0;
}
