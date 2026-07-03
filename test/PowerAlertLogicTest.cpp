#include <iostream>
#include <cassert>
#include <stdint.h>
#include <stdbool.h>
using namespace std;

// Pull in the pure logic header (no Arduino deps)
#include "../src/PowerAlert/PowerAlertLogic.h"
// Pull in the cause enum from Power.h (no Arduino deps for the enum itself)
enum PowerLimitCause : uint8_t {
    POWER_LIMIT_NONE       = 0,
    POWER_LIMIT_BATTERY    = 1 << 0,
    POWER_LIMIT_MOTOR_TEMP = 1 << 1,
    POWER_LIMIT_ESC_TEMP   = 1 << 2,
};

static const uint32_t INTERVAL_MS = 10000;

int main() {
    // Not armed, active causes — should never beep
    {
        PowerAlertLogic logic;
        assert(!logic.update(false, POWER_LIMIT_BATTERY, 0, INTERVAL_MS));
        assert(!logic.update(false, POWER_LIMIT_BATTERY, 5000, INTERVAL_MS));
        assert(!logic.update(false, POWER_LIMIT_BATTERY, 15000, INTERVAL_MS));
        cout << "PASS: not armed never beeps\n";
    }

    // Armed, no causes — should never beep
    {
        PowerAlertLogic logic;
        assert(!logic.update(true, POWER_LIMIT_NONE, 0, INTERVAL_MS));
        assert(!logic.update(true, POWER_LIMIT_NONE, 15000, INTERVAL_MS));
        cout << "PASS: armed but no causes never beeps\n";
    }

    // Armed + cause: beep immediately on entry
    {
        PowerAlertLogic logic;
        assert(logic.update(true, POWER_LIMIT_MOTOR_TEMP, 1000, INTERVAL_MS));
        cout << "PASS: entry beep fires immediately\n";
    }

    // No re-beep before interval
    {
        PowerAlertLogic logic;
        logic.update(true, POWER_LIMIT_MOTOR_TEMP, 0, INTERVAL_MS);  // entry
        assert(!logic.update(true, POWER_LIMIT_MOTOR_TEMP, 100, INTERVAL_MS));
        assert(!logic.update(true, POWER_LIMIT_MOTOR_TEMP, 9999, INTERVAL_MS));
        cout << "PASS: no re-beep before interval\n";
    }

    // Beep again exactly at interval
    {
        PowerAlertLogic logic;
        logic.update(true, POWER_LIMIT_MOTOR_TEMP, 0, INTERVAL_MS);   // entry at t=0
        assert(!logic.update(true, POWER_LIMIT_MOTOR_TEMP, 9999, INTERVAL_MS));
        assert(logic.update(true, POWER_LIMIT_MOTOR_TEMP, 10000, INTERVAL_MS));
        cout << "PASS: beep again at interval\n";
    }

    // Periodic: beep again at 2x interval
    {
        PowerAlertLogic logic;
        logic.update(true, POWER_LIMIT_MOTOR_TEMP, 0, INTERVAL_MS);    // entry t=0
        logic.update(true, POWER_LIMIT_MOTOR_TEMP, 10000, INTERVAL_MS); // periodic t=10s
        assert(!logic.update(true, POWER_LIMIT_MOTOR_TEMP, 19999, INTERVAL_MS));
        assert(logic.update(true, POWER_LIMIT_MOTOR_TEMP, 20000, INTERVAL_MS));
        cout << "PASS: periodic continues at 2x interval\n";
    }

    // Causes clear -> no more beeps, state reset
    {
        PowerAlertLogic logic;
        logic.update(true, POWER_LIMIT_ESC_TEMP, 0, INTERVAL_MS);     // entry
        assert(!logic.update(true, POWER_LIMIT_NONE, 100, INTERVAL_MS)); // causes gone
        assert(!logic.update(true, POWER_LIMIT_NONE, 15000, INTERVAL_MS));
        cout << "PASS: causes clear stops beeps\n";
    }

    // Re-entry after clearing: beeps immediately again
    {
        PowerAlertLogic logic;
        logic.update(true, POWER_LIMIT_BATTERY, 0, INTERVAL_MS);       // entry
        logic.update(true, POWER_LIMIT_NONE, 1000, INTERVAL_MS);       // clear
        assert(logic.update(true, POWER_LIMIT_BATTERY, 2000, INTERVAL_MS)); // re-entry
        cout << "PASS: re-entry beeps immediately\n";
    }

    // Multi-cause treated as a single limited state
    {
        PowerAlertLogic logic;
        uint8_t multi = POWER_LIMIT_BATTERY | POWER_LIMIT_MOTOR_TEMP | POWER_LIMIT_ESC_TEMP;
        assert(logic.update(true, multi, 0, INTERVAL_MS));              // entry
        assert(!logic.update(true, multi, 5000, INTERVAL_MS));          // mid-interval
        assert(logic.update(true, multi, 10000, INTERVAL_MS));          // periodic
        cout << "PASS: multi-cause single limited state\n";
    }

    // Disarm mid-limited: stops immediately, re-arm re-entry beeps
    {
        PowerAlertLogic logic;
        logic.update(true, POWER_LIMIT_MOTOR_TEMP, 0, INTERVAL_MS);    // entry
        assert(!logic.update(false, POWER_LIMIT_MOTOR_TEMP, 5000, INTERVAL_MS)); // disarm
        assert(!logic.update(false, POWER_LIMIT_MOTOR_TEMP, 15000, INTERVAL_MS));
        assert(logic.update(true, POWER_LIMIT_MOTOR_TEMP, 20000, INTERVAL_MS));  // re-arm
        cout << "PASS: disarm stops; re-arm triggers entry beep\n";
    }

    cout << "\nAll tests passed.\n";
    return 0;
}
