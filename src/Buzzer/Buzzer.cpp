#include "Buzzer.h"

Buzzer::Buzzer(uint8_t buzzerPin) :
  pin(buzzerPin),
  active(false),
  startTime(0),
  beepDuration(0),
  pauseDuration(0),
  repetitions(0),
  currentRep(0) {}

void Buzzer::setup() {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
}

void Buzzer::handle() {
  if (!active || repetitions == 0) return;

  uint32_t currentTime = millis();
  uint32_t elapsed = currentTime - startTime;

  if (digitalRead(pin) == HIGH) {
    if (elapsed >= beepDuration) {
      digitalWrite(pin, LOW);
      startTime = currentTime;
      if (++currentRep >= repetitions) {
        active = false;
        repetitions = 0;
      }
    }
  } else {
    if (elapsed >= pauseDuration) {
      if (currentRep < repetitions) {
        digitalWrite(pin, HIGH);
        startTime = currentTime;
      }
    }
  }
}

void Buzzer::startBeep(uint16_t duration, uint8_t reps, uint16_t pause) {
  beepDuration = duration;
  pauseDuration = pause;
  repetitions = reps;
  currentRep = 0;
  active = true;
  digitalWrite(pin, HIGH);
  startTime = millis();
}

void Buzzer::beepSuccess() {
  startBeep(200, 1, 0);
}

void Buzzer::beepError() {
  startBeep(500, 3, 300);
}

void Buzzer::beepWarning() {
  startBeep(300, 2, 200);
}

void Buzzer::beepCustom(uint16_t duration, uint8_t repetitions) {
  startBeep(duration, repetitions, 100);
}

void Buzzer::stop() {
  digitalWrite(pin, LOW);
  active = false;
  repetitions = 0;
}