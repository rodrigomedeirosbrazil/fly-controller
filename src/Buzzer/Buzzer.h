#ifndef BUZZER_H
#define BUZZER_H

#include <Arduino.h>

class Buzzer {
  private:
    uint8_t pin;
    bool playing;
    uint32_t startTime;
    uint16_t beepDuration;
    uint16_t pauseDuration;
    uint8_t repetitions;
    uint8_t currentRepetition;
    bool isOn;
    
    void startBeep(uint16_t duration, uint8_t reps, uint16_t pause);
    bool isPlaying() { return playing; };
    
  public:
    Buzzer(uint8_t buzzerPin);
    void setup();
    void handle();
    void beepSuccess();
    void beepError();
    void beepWarning();
    void beepCustom(uint16_t duration, uint8_t repetitions);
    void stop();
};

#endif