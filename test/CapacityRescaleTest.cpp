// test/CapacityRescaleTest.cpp
// Host-only unit test for the proportional capacity rescale. Compile with:
//   c++ -std=c++17 test/CapacityRescaleTest.cpp -o "${TMPDIR:-/tmp}/capacity_rescale_test"
#include <cassert>
#include <cstdint>
#include <iostream>
#include "../src/BatteryMonitor/CapacityRescale.h"
using namespace std;

void test_keeps_percentage() {
    assert(rescaleRemainingMah(6000, 10000, 20000) == 12000);   // 60 % stays 60 %
    assert(rescaleRemainingMah(9000, 18000, 34000) == 17000);   // 50 %
    assert(rescaleRemainingMah(34000, 34000, 18000) == 18000);  // full stays full
    assert(rescaleRemainingMah(0, 18000, 34000) == 0);          // empty stays empty
    cout << "test_keeps_percentage passed\n";
}

void test_unknown_old_capacity_means_full() {
    assert(rescaleRemainingMah(0, 0, 18000) == 18000);
    assert(rescaleRemainingMah(5000, 0, 18000) == 18000);
    cout << "test_unknown_old_capacity_means_full passed\n";
}

void test_clamped_to_new_capacity() {
    // remaining above capacity should not happen, but must not overshoot.
    assert(rescaleRemainingMah(20000, 10000, 18000) == 18000);
    cout << "test_clamped_to_new_capacity passed\n";
}

void test_no_overflow_at_largest_values() {
    // 65000 * 65000 fits in 32 bits; 64999 * 100000 does not, so the 64-bit intermediate is required.
    assert(rescaleRemainingMah(65000, 65000, 65000) == 65000);
    assert(rescaleRemainingMah(64999, 65000, 100000) == 99998);
    cout << "test_no_overflow_at_largest_values passed\n";
}

int main() {
    test_keeps_percentage();
    test_unknown_old_capacity_means_full();
    test_clamped_to_new_capacity();
    test_no_overflow_at_largest_values();
    cout << "CapacityRescaleTest: all passed\n";
    return 0;
}
