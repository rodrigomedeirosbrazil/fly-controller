#ifndef REMOTE_PAIRING_H
#define REMOTE_PAIRING_H

#include <stdint.h>

class RemotePairing {
  public:
    void load();                       // read saved controller MAC from NVS
    bool isPaired() const { return paired_; }
    const uint8_t *peerMac() const { return mac_; }
    void save(const uint8_t mac[6]);   // persist + mark paired
    void clear();                      // forget pairing

  private:
    uint8_t mac_[6] = {0};
    bool paired_ = false;
};

extern RemotePairing remotePairing;

#endif // REMOTE_PAIRING_H
