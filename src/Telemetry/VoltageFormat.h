#pragma once
#include <stdint.h>
#include <stdio.h>

// Pack voltage as the `V.mmm` text both CSV surfaces emit: the $XCTOD
// sentence (src/Xctod/) and the flight log (src/TelemetryLogger/), which are
// kept field-for-field aligned. Host-tested in test/VoltageFormatTest.cpp.
//
// The decimal is ALWAYS three digits. Both call sites used to build it as
// "%u." + an optional single "0" + "%u", which is only correct when
// millivolts % 1000 happens to be 0 or >= 100. Everything in 1-99 lost the
// leading zeros and shifted the decimal point: 50040 mV went out as "50.40",
// read back as 50.4 V -- a +0.36 V error, and up to +0.891 V at 50099. That
// is roughly a tenth of all readings, appearing and vanishing as the pack
// drifts across each 100 mV boundary, on a number XCTrack shows the pilot in
// flight. docs/XCTOD-PROTOCOL.md has always documented the field as V.mmm.
inline void formatMilliVolts(char* out, size_t cap, uint16_t millivolts) {
    if (out == nullptr || cap == 0) {
        return;
    }
    snprintf(out, cap, "%u.%03u",
             (unsigned) (millivolts / 1000),
             (unsigned) (millivolts % 1000));
}
