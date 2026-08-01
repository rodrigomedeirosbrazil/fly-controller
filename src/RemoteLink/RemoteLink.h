#ifndef REMOTE_LINK_H
#define REMOTE_LINK_H

#include <stdint.h>
#include "RemoteLinkProtocol.h"

// Host-testable: true if lastRxMs is within windowMs of nowMs.
// gap > 0x80000000UL means the recv callback updated lastRxMs after the
// caller's millis() snapshot (an ISR race, since lastRxMs is volatile and
// written from the ESP-NOW receive callback); treat that as "just received",
// not as an underflowed multi-year-old gap.
inline bool isGapFresh(uint32_t lastRxMs, uint32_t nowMs, uint32_t windowMs) {
    uint32_t gap = nowMs - lastRxMs;
    if (gap > 0x80000000UL) return true;
    return gap < windowMs;
}

class RemoteLink {
  public:
    void setup();   // init ESP-NOW on the AP channel; load paired peer from Settings
    void handle();  // periodic TX of controller state to the remote (~5 Hz)

    // Throttle / button source (used by Throttle ReadFn and the Button config).
    uint16_t lastHallRaw() const { return rx_.hallRaw; }
    bool remoteButtonPressed() const { return rx_.buttonPressed != 0; }
    uint32_t lastRxMs() const { return lastRxMs_; }
    bool hasState() const { return hasState_; }

    // Raw per-tick freshness of the throttle link: true only if we have ever
    // heard from the remote AND the last packet arrived within the last
    // kFreshWindowMs. The remote sends ~50 Hz (one packet every ~20 ms), so
    // 100 ms tolerates a handful of dropped packets — comfortably smaller
    // than ThrottleSignalLogic's 500 ms debounce, so feeding this in every
    // ~10 ms Throttle::handle() tick tracks the real packet gap closely.
    // This replaces the old computeFailsafe()/FailsafeAction split: the
    // 500 ms/3 s decision now lives in ThrottleSignalLogic's wireless config.
    bool isLinkFresh(uint32_t nowMs) const {
        return hasState_ && isGapFresh(lastRxMs_, nowMs, kFreshWindowMs);
    }

    // Controller -> remote state.
    void setArmed(bool armed) { tx_.armed = armed ? 1 : 0; }
    void setCalibrating(bool c) { tx_.calibrating = c ? 1 : 0; }
    void requestBeep(uint8_t beep);   // bumps the beep counter; sent on next handle()

    // Pairing.
    void enterPairing() { pairing_ = true; }
    bool isPairing() const { return pairing_; }

    // Called from the static ESP-NOW recv trampoline.
    void onReceive(const uint8_t *senderMac, const uint8_t *data, int len);

  private:
    void addPeer(const uint8_t mac[6]);
    void sendState();

    static const uint32_t kFreshWindowMs = 100;

    ThrottleToControllerPacket rx_{};
    ControllerToThrottlePacket tx_{};
    volatile bool hasState_ = false;
    volatile uint32_t lastRxMs_ = 0;
    uint8_t peerMac_[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    bool pairing_ = false;
    uint32_t lastTxMs_ = 0;
};

extern RemoteLink remoteLink;

#endif // REMOTE_LINK_H
