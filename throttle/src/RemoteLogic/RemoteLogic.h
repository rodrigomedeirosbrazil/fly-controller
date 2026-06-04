#ifndef REMOTE_LOGIC_H
#define REMOTE_LOGIC_H

#include <stdint.h>
#include "RemoteLinkProtocol.h"

// Tunables (shared by firmware and tests).
#define REMOTE_BLINK_PERIOD_MS 250  // half-cycle of every blink pattern
#define REMOTE_LINK_TIMEOUT_MS 1000 // no controller packet for this long = link lost

// LED display patterns, highest priority first (see pickLedPattern).
enum class RemoteLedPattern {
    PairingAlternate, // red <-> green alternating
    UnpairedBlink,    // red blinking, green off
    LinkLostBlink,    // red + green blinking together
    ArmedSolid,       // red solid
    DisarmedSolid,    // green solid
};

struct LedOutput {
    bool red;
    bool green;
};

// Holds the monotonic button-event counter sent to the controller.
struct RemoteButtonState {
    uint8_t counter = 0;
    uint8_t type = RemoteButtonEvent::None;
};

// Choose the LED pattern from the current remote state. Priority:
// pairing > unpaired > link-lost > armed/disarmed.
inline RemoteLedPattern pickLedPattern(bool paired, bool pairingMode, bool linkLost, bool armed) {
    if (pairingMode) return RemoteLedPattern::PairingAlternate;
    if (!paired)     return RemoteLedPattern::UnpairedBlink;
    if (linkLost)    return RemoteLedPattern::LinkLostBlink;
    return armed ? RemoteLedPattern::ArmedSolid : RemoteLedPattern::DisarmedSolid;
}

// Render a pattern to concrete LED on/off states at time nowMs.
inline LedOutput renderLeds(RemoteLedPattern pattern, uint32_t nowMs) {
    bool phase = (nowMs / REMOTE_BLINK_PERIOD_MS) % 2; // toggles every period
    switch (pattern) {
        case RemoteLedPattern::ArmedSolid:       return {true, false};
        case RemoteLedPattern::DisarmedSolid:    return {false, true};
        case RemoteLedPattern::UnpairedBlink:    return {phase, false};
        case RemoteLedPattern::LinkLostBlink:    return {phase, phase};
        case RemoteLedPattern::PairingAlternate: return {phase, !phase};
    }
    return {false, false};
}

// True once no controller packet has arrived for REMOTE_LINK_TIMEOUT_MS.
inline bool isLinkLost(uint32_t lastRxMs, uint32_t nowMs) {
    return (nowMs - lastRxMs) > REMOTE_LINK_TIMEOUT_MS;
}

// Record a discrete button event: bump the monotonic counter (wraps) and store type.
inline void recordButtonEvent(RemoteButtonState &state, uint8_t eventType) {
    state.counter++; // uint8_t wraps 255 -> 0 naturally
    state.type = eventType;
}

#endif // REMOTE_LOGIC_H
