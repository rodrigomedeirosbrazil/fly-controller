#pragma once

// Pure BLE link-policy decisions for BleServerHost. No Arduino/IDF includes so
// the rules can be host-tested (test/BleLinkPolicyTest.cpp).

#include <cstdint>
#include <cstring>

namespace BleLinkPolicy {

// Connection parameters in BLE units: intervals 1.25 ms, timeout 10 ms.
struct ConnParams {
    uint16_t minInterval;
    uint16_t maxInterval;
    uint16_t latency;
    uint16_t timeout;
};

constexpr ConnParams kPhoneConnParams{24, 36, 0, 600};  // 30-45 ms, 6 s timeout

constexpr uint16_t kAdvIntervalIdleMin = 0x20;
constexpr uint16_t kAdvIntervalIdleMax = 0x40;
constexpr uint16_t kAdvIntervalConnectedMin = 0x190;
constexpr uint16_t kAdvIntervalConnectedMax = 0x280;

constexpr uint8_t kLinkRolePeripheral = 1;  // 0 = we are central (BMS link)

constexpr uint8_t kMaxPeripheralLinks = 4;  // CONFIG_BT_ACL_CONNECTIONS

constexpr uint32_t kIntervalUnitUs = 1250;
constexpr uint32_t kTimeoutUnitUs = 10000;
constexpr uint32_t kAppleMinIntervalUnits = 12;  // 15 ms
constexpr uint32_t kAppleMaxLatency = 30;
constexpr uint32_t kAppleMinTimeoutUnits = 600;   // 6 s
constexpr uint32_t kAppleMaxTimeoutUnits = 1800;  // 18 s
constexpr uint32_t kAppleMaxEffectiveIntervalUs = 6000000;

// Apple Accessory Design Guidelines section 57.6 rules.
inline bool meetsAppleRules(const ConnParams& p) {
    const uint32_t minI = p.minInterval;
    const uint32_t maxI = p.maxInterval;
    const uint32_t lat = p.latency;
    const uint32_t tmo = p.timeout;
    if (!(minI >= kAppleMinIntervalUnits && minI % kAppleMinIntervalUnits == 0)) return false;
    if (!((maxI == minI && minI == kAppleMinIntervalUnits) || maxI >= minI + kAppleMinIntervalUnits)) return false;
    if (lat > kAppleMaxLatency) return false;
    if (tmo < kAppleMinTimeoutUnits || tmo > kAppleMaxTimeoutUnits) return false;
    const uint32_t effectiveUs = maxI * kIntervalUnitUs * (lat + 1);
    if (effectiveUs > kAppleMaxEffectiveIntervalUs) return false;
    if (!(tmo * kTimeoutUnitUs > effectiveUs * 3)) return false;
    return true;
}

struct AdvInterval {
    uint16_t min;
    uint16_t max;
};

inline AdvInterval advIntervalFor(uint8_t peripheralLinks) {
    if (peripheralLinks == 0) return {kAdvIntervalIdleMin, kAdvIntervalIdleMax};
    return {kAdvIntervalConnectedMin, kAdvIntervalConnectedMax};
}

// Peripheral-role (phone) links keyed by 6-byte address.
class PeripheralLinks {
public:
    void onConnect(const uint8_t bda[6], uint8_t linkRole) {
        if (linkRole != kLinkRolePeripheral) return;
        if (find(bda) >= 0) return;
        for (uint8_t i = 0; i < kMaxPeripheralLinks; i++) {
            if (!used_[i]) {
                used_[i] = true;
                reRequested_[i] = false;
                memcpy(addr_[i], bda, 6);
                return;
            }
        }
    }

    // True when this was the last peripheral link.
    bool onDisconnect(const uint8_t bda[6], uint8_t linkRole) {
        if (linkRole != kLinkRolePeripheral) return false;
        int i = find(bda);
        if (i < 0) return false;
        used_[i] = false;
        reRequested_[i] = false;
        return count() == 0;
    }

    uint8_t count() const {
        uint8_t n = 0;
        for (uint8_t i = 0; i < kMaxPeripheralLinks; i++) {
            if (used_[i]) n++;
        }
        return n;
    }

    // True at most once per link, when the applied timeout is under the phone target.
    bool shouldReRequest(const uint8_t bda[6], uint16_t appliedTimeout) {
        if (appliedTimeout >= kPhoneConnParams.timeout) return false;
        int i = find(bda);
        if (i < 0 || reRequested_[i]) return false;
        reRequested_[i] = true;
        return true;
    }

private:
    int find(const uint8_t bda[6]) const {
        for (uint8_t i = 0; i < kMaxPeripheralLinks; i++) {
            if (used_[i] && memcmp(addr_[i], bda, 6) == 0) return i;
        }
        return -1;
    }

    bool used_[kMaxPeripheralLinks] = {};
    bool reRequested_[kMaxPeripheralLinks] = {};
    uint8_t addr_[kMaxPeripheralLinks][6] = {};
};

}  // namespace BleLinkPolicy
