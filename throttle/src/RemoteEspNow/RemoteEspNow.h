#ifndef REMOTE_ESP_NOW_H
#define REMOTE_ESP_NOW_H

#include <stdint.h>
#include "RemoteLinkProtocol.h"

class RemoteEspNow {
  public:
    void setup();                                   // init WiFi STA + ESP-NOW on REMOTE_LINK_CHANNEL
    void sendThrottle(const ThrottleToControllerPacket &pkt); // unicast to peer (or broadcast if unpaired)
    bool hasState() const { return hasState_; }     // received at least one controller packet
    ControllerToThrottlePacket state() const { return state_; }
    uint32_t lastRxMs() const { return lastRxMs_; }
    void setPeer(const uint8_t mac[6]);             // set/replace the controller peer
    void setBroadcastPeer();                        // peer = ff:ff:ff:ff:ff:ff (unpaired/pairing)

    // Called from the static ESP-NOW recv trampoline.
    void onReceive(const uint8_t *senderMac, const uint8_t *data, int len);

    const uint8_t *lastSenderMac() const { return lastSenderMac_; }

  private:
    ControllerToThrottlePacket state_{};
    volatile bool hasState_ = false;
    volatile uint32_t lastRxMs_ = 0;
    uint8_t peerMac_[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t lastSenderMac_[6] = {0};
};

extern RemoteEspNow remoteEspNow;

#endif // REMOTE_ESP_NOW_H
