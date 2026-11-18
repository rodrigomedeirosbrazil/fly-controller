#pragma once

#include <cstdint>

// Preferred connection parameters for the BMS link (we are the central).
// The BMS is polled about every 2 s, so the 12.5-25 ms default interval only
// burned airtime that competed with the phone link on the shared radio.
// Units: intervals in 1.25 ms steps, timeout in 10 ms steps.
constexpr uint16_t BMS_CONN_MIN_INTERVAL = 48;  // 60 ms
constexpr uint16_t BMS_CONN_MAX_INTERVAL = 80;  // 100 ms
constexpr uint16_t BMS_CONN_LATENCY = 0;
constexpr uint16_t BMS_CONN_TIMEOUT = 600;      // 6 s

#ifdef ARDUINO
#include <BLEAddress.h>
#include <esp_gap_ble_api.h>

// Call right before BLEClient::connect() so the stack opens the link with
// these parameters instead of its defaults.
inline void applyBmsPreferredConnParams(BLEAddress& addr) {
    esp_ble_gap_set_prefer_conn_params(*addr.getNative(), BMS_CONN_MIN_INTERVAL,
                                       BMS_CONN_MAX_INTERVAL, BMS_CONN_LATENCY,
                                       BMS_CONN_TIMEOUT);
}
#endif
