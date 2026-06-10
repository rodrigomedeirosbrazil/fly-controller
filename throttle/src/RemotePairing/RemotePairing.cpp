#include <Preferences.h>
#include <string.h>
#include "RemotePairing.h"

RemotePairing remotePairing;

static const char *NVS_NAMESPACE = "remote";
static const char *NVS_KEY_MAC = "ctrlMac";

void RemotePairing::load() {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true); // read-only
    size_t len = prefs.getBytes(NVS_KEY_MAC, mac_, sizeof(mac_));
    prefs.end();
    paired_ = (len == sizeof(mac_));
}

void RemotePairing::save(const uint8_t mac[6]) {
    memcpy(mac_, mac, sizeof(mac_));
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false); // read-write
    prefs.putBytes(NVS_KEY_MAC, mac_, sizeof(mac_));
    prefs.end();
    paired_ = true;
}

void RemotePairing::clear() {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.remove(NVS_KEY_MAC);
    prefs.end();
    paired_ = false;
    memset(mac_, 0, sizeof(mac_));
}
