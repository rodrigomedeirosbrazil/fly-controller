#include <Arduino.h>
#include <AceButton.h>
#include <Buzzer.h>
#include "RemoteLinkProtocol.h"
#include "RemotePins.h"
#include "RemoteLogic/RemoteLogic.h"
#include "RemoteEspNow/RemoteEspNow.h"

using namespace ace_button;

Buzzer buzzer(BUZZER_PIN);
AceButton button(BUTTON_PIN);
RemoteButtonState buttonState;

static uint8_t lastBeepCounter = 0;

static const uint16_t THROTTLE_TX_INTERVAL_MS = 20; // ~50 Hz
static uint32_t lastTxMs = 0;

void handleButton(AceButton *, uint8_t eventType, uint8_t) {
    if (eventType == AceButton::kEventClicked) {
        recordButtonEvent(buttonState, RemoteButtonEvent::ShortClick);
    } else if (eventType == AceButton::kEventLongPressed) {
        recordButtonEvent(buttonState, RemoteButtonEvent::LongPress);
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_RED_PIN, OUTPUT);
    pinMode(LED_GREEN_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    analogReadResolution(12); // 0..4095 raw Hall

    buzzer.setup();

    ButtonConfig *cfg = button.getButtonConfig();
    cfg->setEventHandler(handleButton);
    cfg->setFeature(ButtonConfig::kFeatureClick);
    cfg->setFeature(ButtonConfig::kFeatureLongPress);
    cfg->setFeature(ButtonConfig::kFeatureSuppressAfterLongPress);

    remoteEspNow.setup();
    buzzer.beepSystemStart();
}

static void driveLeds() {
    bool paired = false; // wired up in Task 4 (pairing/persistence)
    bool pairingMode = false;
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
    button.check();
    buzzer.handle();

    uint32_t now = millis();
    if (now - lastTxMs >= THROTTLE_TX_INTERVAL_MS) {
        lastTxMs = now;
        ThrottleToControllerPacket pkt{};
        pkt.hallRaw = analogRead(HALL_PIN);
        pkt.buttonEventCounter = buttonState.counter;
        pkt.buttonEventType = buttonState.type;
        remoteEspNow.sendThrottle(pkt);
    }

    playRequestedBeep();
    driveLeds();
}
