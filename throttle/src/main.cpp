#include <Arduino.h>
#include <Buzzer.h>
#include "RemoteLinkProtocol.h"
#include "RemotePins.h"
#include "RemoteLogic/RemoteLogic.h"
#include "RemoteEspNow/RemoteEspNow.h"
#include "RemotePairing/RemotePairing.h"

Buzzer buzzer(BUZZER_PIN);

static uint8_t lastBeepCounter = 0;
static bool pairingMode = false;

static const uint16_t THROTTLE_TX_INTERVAL_MS = 20;   // ~50 Hz
static const uint32_t PAIRING_HOLD_MS = 3000;         // hold (unpaired) to enter pairing
static uint32_t lastTxMs = 0;
static uint32_t pressStartMs = 0;
static bool wasPressed = false;

// Active-low button: pressed when the pin reads LOW.
static bool buttonPressed() {
    return digitalRead(BUTTON_PIN) == LOW;
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_RED_PIN, OUTPUT);
    pinMode(LED_GREEN_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    analogReadResolution(12); // 0..4095 raw Hall

    buzzer.setup();

    remoteEspNow.setup();
    remotePairing.load();
    if (remotePairing.isPaired()) {
        remoteEspNow.setPeer(remotePairing.peerMac());
    } else {
        remoteEspNow.setBroadcastPeer();
    }
    buzzer.beepSystemStart();
}

// Button handling: immediate press feedback + hold-to-pair (when unpaired).
static void updateButton(bool pressed, uint32_t now) {
    if (pressed && !wasPressed) {
        pressStartMs = now;
        buzzer.beepButtonClick(); // immediate local feedback on every press
    }
    if (pressed && !remotePairing.isPaired() && !pairingMode &&
        (now - pressStartMs) >= PAIRING_HOLD_MS) {
        pairingMode = true;
        remoteEspNow.setBroadcastPeer();
        buzzer.beepCalibrationStep(); // entered pairing mode
    }
    wasPressed = pressed;
}

static void driveLeds() {
    bool paired = remotePairing.isPaired();
    bool linkLost = !remoteEspNow.hasState() || isLinkLost(remoteEspNow.lastRxMs(), millis());
    bool armed = remoteEspNow.hasState() && remoteEspNow.state().armed;
    LedOutput out = renderLeds(pickLedPattern(paired, pairingMode, linkLost, armed), millis());
    digitalWrite(LED_RED_PIN, out.red ? HIGH : LOW);
    digitalWrite(LED_GREEN_PIN, out.green ? HIGH : LOW);
}

static void playRequestedBeep() {
    if (!remoteEspNow.hasState()) return;
    ControllerToThrottlePacket s = remoteEspNow.state();
    if (!remoteLinkCounterAdvanced(s.beepCommandCounter, lastBeepCounter)) return;
    switch (s.beepCommand) {
        case RemoteBeep::SystemStart:     buzzer.beepSystemStart(); break;
        case RemoteBeep::CalibrationStep: buzzer.beepCalibrationStep(); break;
        case RemoteBeep::Armed:           buzzer.beepArmedAlert(); break;
        case RemoteBeep::Disarmed:        buzzer.beepDisarmed(); break;
        case RemoteBeep::Alert:           buzzer.beepArmingBlocked(); break;
        default: break;
    }
}

void loop() {
    buzzer.handle();

    uint32_t now = millis();
    bool pressed = buttonPressed();
    updateButton(pressed, now);

    if (now - lastTxMs >= THROTTLE_TX_INTERVAL_MS) {
        lastTxMs = now;
        ThrottleToControllerPacket pkt{};
        pkt.hallRaw = analogRead(HALL_PIN);
        pkt.buttonPressed = pressed ? 1 : 0;
        remoteEspNow.sendThrottle(pkt);
    }

    // While pairing, the first controller packet teaches us its MAC.
    if (pairingMode && remoteEspNow.hasState()) {
        remotePairing.save(remoteEspNow.lastSenderMac());
        remoteEspNow.setPeer(remotePairing.peerMac());
        pairingMode = false;
        buzzer.beepCalibrationComplete(); // audible pairing confirmation
    }

    playRequestedBeep();
    driveLeds();
}
