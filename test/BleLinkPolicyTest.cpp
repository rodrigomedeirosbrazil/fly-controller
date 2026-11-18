#include <cassert>
#include <cstdint>
#include <cstdio>
#include "../src/BleServerHost/BleLinkPolicy.h"

using namespace BleLinkPolicy;

static const uint8_t A[6] = {1, 2, 3, 4, 5, 6};
static const uint8_t B[6] = {1, 2, 3, 4, 5, 7};
static const uint8_t C[6] = {9, 9, 9, 9, 9, 9};
static const uint8_t BMS[6] = {0xAA, 0, 0, 0, 0, 1};

static void test_phone_params_meet_apple_rules() {
    assert(meetsAppleRules(kPhoneConnParams));
    assert(!meetsAppleRules({24, 24, 0, 600}));
    assert(!meetsAppleRules({20, 36, 0, 600}));
    assert(!meetsAppleRules({24, 36, 0, 500}));
    assert(!meetsAppleRules({24, 36, 31, 600}));
}

static void test_adv_interval() {
    AdvInterval i0 = advIntervalFor(0);
    assert(i0.min == 0x20 && i0.max == 0x40);
    AdvInterval i1 = advIntervalFor(1);
    assert(i1.min == 0x190 && i1.max == 0x280);
    AdvInterval i3 = advIntervalFor(3);
    assert(i3.min == 0x190 && i3.max == 0x280);
}

static void test_central_links_ignored() {
    PeripheralLinks l;
    l.onConnect(BMS, 0);
    assert(l.count() == 0);
    assert(!l.onDisconnect(BMS, 0));
}

static void test_last_peripheral_leaving() {
    PeripheralLinks l;
    l.onConnect(A, kLinkRolePeripheral);
    l.onConnect(B, kLinkRolePeripheral);
    assert(l.count() == 2);
    assert(!l.onDisconnect(A, kLinkRolePeripheral));
    assert(l.onDisconnect(B, kLinkRolePeripheral));
    assert(l.count() == 0);
}

static void test_duplicate_and_unknown() {
    PeripheralLinks l;
    l.onConnect(A, kLinkRolePeripheral);
    l.onConnect(A, kLinkRolePeripheral);
    assert(l.count() == 1);
    assert(!l.onDisconnect(C, kLinkRolePeripheral));
    assert(l.count() == 1);
}

static void test_capacity() {
    PeripheralLinks l;
    uint8_t addrs[5][6];
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 6; j++) addrs[i][j] = (uint8_t)(i + 1);
        l.onConnect(addrs[i], kLinkRolePeripheral);
    }
    assert(l.count() == 4);
    for (int i = 0; i < 3; i++) assert(!l.onDisconnect(addrs[i], kLinkRolePeripheral));
    assert(l.onDisconnect(addrs[3], kLinkRolePeripheral));
    assert(l.count() == 0);
}

static void test_rerequest_only_once() {
    PeripheralLinks l;
    l.onConnect(A, kLinkRolePeripheral);
    assert(l.shouldReRequest(A, 72));
    assert(!l.shouldReRequest(A, 72));
    assert(!l.shouldReRequest(A, 600));
    assert(!l.shouldReRequest(C, 72));
    l.onDisconnect(A, kLinkRolePeripheral);
    l.onConnect(A, kLinkRolePeripheral);
    assert(l.shouldReRequest(A, 72));
}

int main() {
    test_phone_params_meet_apple_rules();
    test_adv_interval();
    test_central_links_ignored();
    test_last_peripheral_leaving();
    test_duplicate_and_unknown();
    test_capacity();
    test_rerequest_only_once();
    printf("BleLinkPolicyTest: all passed\n");
    return 0;
}
