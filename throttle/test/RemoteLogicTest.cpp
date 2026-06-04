#include <cassert>
#include <cstdint>
#include <iostream>
#include "../src/RemoteLogic/RemoteLogic.h"
using namespace std;

void test_led_pattern_priority() {
    // Pairing mode wins over everything.
    assert(pickLedPattern(/*paired*/true, /*pairing*/true, /*linkLost*/true, /*armed*/true)
           == RemoteLedPattern::PairingAlternate);
    // Not paired -> unpaired blink.
    assert(pickLedPattern(false, false, false, false) == RemoteLedPattern::UnpairedBlink);
    // Paired but link lost.
    assert(pickLedPattern(true, false, true, true) == RemoteLedPattern::LinkLostBlink);
    // Paired, linked, armed/disarmed solid.
    assert(pickLedPattern(true, false, false, true) == RemoteLedPattern::ArmedSolid);
    assert(pickLedPattern(true, false, false, false) == RemoteLedPattern::DisarmedSolid);
}

void test_led_render_solid() {
    LedOutput armed = renderLeds(RemoteLedPattern::ArmedSolid, 0);
    assert(armed.red == true && armed.green == false);
    LedOutput disarmed = renderLeds(RemoteLedPattern::DisarmedSolid, 12345);
    assert(disarmed.red == false && disarmed.green == true);
}

void test_led_render_blink_phases() {
    // At t=0 the blink phase is "on"; after one period it flips.
    LedOutput unpairedOn = renderLeds(RemoteLedPattern::UnpairedBlink, 0);
    LedOutput unpairedOff = renderLeds(RemoteLedPattern::UnpairedBlink, REMOTE_BLINK_PERIOD_MS);
    assert(unpairedOn.red != unpairedOff.red);
    assert(unpairedOn.green == false && unpairedOff.green == false);

    // Link-lost blinks both together (red == green).
    LedOutput linkLost = renderLeds(RemoteLedPattern::LinkLostBlink, 0);
    assert(linkLost.red == linkLost.green);

    // Pairing alternates (red != green).
    LedOutput pairing = renderLeds(RemoteLedPattern::PairingAlternate, 0);
    assert(pairing.red != pairing.green);
}

void test_link_lost_detection() {
    // No loss within the timeout window.
    assert(isLinkLost(/*lastRxMs*/1000, /*nowMs*/1000 + REMOTE_LINK_TIMEOUT_MS - 1) == false);
    // Loss once the timeout is exceeded.
    assert(isLinkLost(1000, 1000 + REMOTE_LINK_TIMEOUT_MS + 1) == true);
}

void test_button_event_counter_increments_and_wraps() {
    RemoteButtonState s; // counter starts at 0
    assert(s.counter == 0);
    recordButtonEvent(s, RemoteButtonEvent::ShortClick);
    assert(s.counter == 1 && s.type == RemoteButtonEvent::ShortClick);
    s.counter = 255;
    recordButtonEvent(s, RemoteButtonEvent::LongPress);
    assert(s.counter == 0 && s.type == RemoteButtonEvent::LongPress); // wraps
}

int main() {
    test_led_pattern_priority();
    test_led_render_solid();
    test_led_render_blink_phases();
    test_link_lost_detection();
    test_button_event_counter_increments_and_wraps();
    cout << "RemoteLogicTest: all passed" << endl;
    return 0;
}
