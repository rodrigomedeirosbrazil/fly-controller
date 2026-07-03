#include <cassert>
#include <cstdint>
#include <iostream>
#include "../src/RemoteLink/RemoteLinkProtocol.h"
using namespace std;

void test_packet_sizes_are_fixed() {
    // Layout must stay byte-identical across both firmwares.
    assert(sizeof(ThrottleToControllerPacket) == 4);
    assert(sizeof(ControllerToThrottlePacket) == 4);
}

void test_protocol_version_is_declared() {
    // Bump REMOTE_LINK_PROTOCOL_VERSION on both repos whenever the wire
    // format changes — this assertion just keeps the constant from being
    // silently deleted.
    assert(REMOTE_LINK_PROTOCOL_VERSION >= 1);
}

void test_counter_advances_only_on_change() {
    uint8_t lastSeen = 5;
    assert(remoteLinkCounterAdvanced(5, lastSeen) == false); // same -> no action
    assert(remoteLinkCounterAdvanced(6, lastSeen) == true);  // changed -> act
    assert(lastSeen == 6);                                   // updated
    assert(remoteLinkCounterAdvanced(6, lastSeen) == false); // same again
}

void test_counter_wraps_around() {
    uint8_t lastSeen = 255;
    assert(remoteLinkCounterAdvanced(0, lastSeen) == true);  // 255 -> 0 is a change
    assert(lastSeen == 0);
}

int main() {
    test_packet_sizes_are_fixed();
    test_protocol_version_is_declared();
    test_counter_advances_only_on_change();
    test_counter_wraps_around();
    cout << "RemoteLinkProtocolTest: all passed" << endl;
    return 0;
}
