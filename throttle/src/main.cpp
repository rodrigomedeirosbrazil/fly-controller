#include <Arduino.h>
#include <Buzzer.h>
#include "RemoteLinkProtocol.h"
#include "RemotePins.h"

Buzzer buzzer(BUZZER_PIN);

void setup() {
    Serial.begin(115200);
    pinMode(LED_RED_PIN, OUTPUT);
    pinMode(LED_GREEN_PIN, OUTPUT);
    buzzer.setup();
    buzzer.beepSystemStart();
}

void loop() {
    buzzer.handle();
}
