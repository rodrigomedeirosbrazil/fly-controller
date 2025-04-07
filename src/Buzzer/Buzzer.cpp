#include "Buzzer.h"

Buzzer::Buzzer(uint8_t buzzerPin) :
  pin(buzzerPin),
  playing(false),
  startTime(0),
  beepDuration(0),
  pauseDuration(0),
  repetitions(0),
  currentRepetition(0),
  isOn(0) {}

void Buzzer::setup() {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, HIGH);
}

void Buzzer::handle() {
  if (!playing) {
    if (isOn) {
      isOn = false;
      digitalWrite(pin, HIGH);
    }
    return;
  }

  uint32_t currentTime = millis();
  uint32_t elapsed = currentTime - startTime;

  if (isOn) {
    if (elapsed >= beepDuration) {
      digitalWrite(pin, HIGH);
      isOn = false;
      startTime = currentTime;
      if (++currentRepetition >= repetitions) {
        stop();
      }
    }

    return;
  } 
  
  if (elapsed >= pauseDuration) {
    digitalWrite(pin, LOW);
    isOn = true;
    startTime = currentTime;
  }
}

void Buzzer::startBeep(uint16_t duration, uint8_t reps, uint16_t pause) {
  beepDuration = duration;
  pauseDuration = pause;
  repetitions = reps;
  currentRepetition = 0;
  playing = true;
  isOn = true;
  digitalWrite(pin, LOW);
  startTime = millis();
}

void Buzzer::beepSuccess() {
  startBeep(200, 2, 200);
}

void Buzzer::beepError() {
  startBeep(1000, 1, 0);
}

void Buzzer::beepWarning() {
  startBeep(200, 4, 200);
}

void Buzzer::beepCustom(uint16_t duration, uint8_t repetitions) {
  startBeep(duration, repetitions, 100);
}

void Buzzer::stop() {
  digitalWrite(pin, HIGH);
  playing = false;
  isOn = false;
  repetitions = 0;
}