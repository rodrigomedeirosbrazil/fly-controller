#ifndef REMOTE_LINK_H
#define REMOTE_LINK_H

#include <stdint.h>
#include "RemoteLinkProtocol.h"
#include "RemoteLinkLogic.h"

class RemoteLink {
  public:
    void setup();   // init ESP-NOW on the AP channel; load paired peer from Settings
    void handle();  // periodic TX of controller state to the remote (~5 Hz)

    // Throttle / button source (used by Throttle ReadFn and the Button config).
    uint16_t lastHallRaw() const { return rx_.hallRaw; }
    bool remoteButtonPressed() const { return rx_.buttonPressed != 0; }
    uint32_t lastRxMs() const { return lastRxMs_; }
    bool hasState() const { return hasState_; }

    // Failsafe convenience (wireless flag comes from Settings at the call site).
    FailsafeAction failsafe(bool wireless, uint32_t nowMs) const {
        return computeFailsafe(wireless, lastRxMs_, nowMs);
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
