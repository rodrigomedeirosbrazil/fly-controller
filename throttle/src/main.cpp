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
static const uint32_t PAIRING_HOLD_MS = 6000;         // hold to enter pairing (well above the 2 s arm/disarm gesture)
static uint32_t lastTxMs = 0;
static uint32_t pressStartMs = 0;
static bool wasPressed = false;

// Local armed-alert beep management (mirrors controller logic using local hall).
static uint16_t hallIdle = 0;
static bool localArmedBeeping = false;

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
    hallIdle = analogRead(HALL_PIN);
    buzzer.beepSystemStart();
}

// Button handling: immediate press feedback + hold-to-pair.
// Long-press always enters pairing — the 6 s hold is sufficient protection
// against accidental re-pairing during flight.
static void updateButton(bool pressed, uint32_t now) {
    if (pressed && !wasPressed) {
        pressStartMs = now;
        buzzer.beepButtonClick();
    }
    if (pressed && !pairingMode &&
        (now - pressStartMs) >= PAIRING_HOLD_MS) {
        pairingMode = true;
        remoteEspNow.setBroadcastPeer();
        buzzer.beepCalibrationStep();
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

// Manage armed-alert beep locally: beep when armed + throttle idle, stop when
// throttle exceeds 5% of full ADC range above the idle baseline.
static void handleLocalArmedBeep() {
    bool armed = remoteEspNow.hasState() && remoteEspNow.state().armed;
    uint16_t hall = analogRead(HALL_PIN);
    int delta = abs((int)hall - (int)hallIdle);
    bool throttleActive = delta > (4095 * 5 / 100); // 5% of 12-bit range

    if (armed && !throttleActive && !localArmedBeeping) {
        buzzer.beepArmedAlert();
        localArmedBeeping = true;
    }
    if ((!armed || throttleActive) && localArmedBeeping) {
        buzzer.stop();
        localArmedBeeping = false;
    }
}

static void playRequestedBeep() {
    if (!remoteEspNow.hasState()) return;
    ControllerToThrottlePacket s = remoteEspNow.state();
    if (!remoteLinkCounterAdvanced(s.beepCommandCounter, lastBeepCounter)) return;
    switch (s.beepCommand) {
        case RemoteBeep::SystemStart:     buzzer.beepSystemStart(); break;
        case RemoteBeep::CalibrationStep: buzzer.beepCalibrationStep(); break;
        case RemoteBeep::Armed:           break; // managed locally by handleLocalArmedBeep
        case RemoteBeep::Disarmed:        buzzer.beepDisarmed(); break;
        case RemoteBeep::Alert:           buzzer.beepArmingBlocked(); break;
        case RemoteBeep::Stop:            buzzer.stop(); break;
        case RemoteBeep::PowerAlert:      buzzer.beepPowerAlert(); break;
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
    handleLocalArmedBeep();
    driveLeds();
}
