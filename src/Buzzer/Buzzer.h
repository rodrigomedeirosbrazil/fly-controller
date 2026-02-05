#ifndef BUZZER_H
#define BUZZER_H

#include <Arduino.h>
#include <driver/ledc.h>

// Structure for a single note
struct Note {
  uint16_t frequency;  // Frequency in Hz
  uint16_t duration;  // Duration in ms
};

// Structure for a melody (sequence of notes)
struct Melody {
  const Note* notes;
  uint8_t noteCount;
  uint16_t pauseBetweenNotes;  // Pause between notes in ms
};

class Buzzer {
  private:
    uint8_t pin;
    uint8_t pwmChannel;
    uint32_t pwmFrequency;
    uint8_t pwmResolution;
    uint32_t pwmDutyCycle;
    bool playing;
    uint32_t startTime;
    uint16_t beepDuration;
    uint16_t pauseDuration;
    uint8_t repetitions;
    uint8_t currentRepetition;
    bool isOn;

    // Melody/sequence support
    bool playingMelody;
    const Melody* currentMelody;
    uint8_t currentNoteIndex;
    uint32_t noteStartTime;
    bool noteIsOn;

    void startBeep(uint16_t duration, uint8_t reps, uint16_t pause, uint16_t frequency = 0);
    void startMelody(const Melody* melody);
    bool isPlaying() { return playing || playingMelody; };
    void setPwmOn();
    void setPwmOff();
    void setFrequency(uint16_t frequency);

  public:
    Buzzer(uint8_t buzzerPin);
    void setup();
    void handle();

    // Contextual methods
    void beepSystemStart();
    void beepCalibrationStep();
    void beepCalibrationComplete();
    void beepDisarmed();
    void beepArmingBlocked();
    void beepButtonClick();
    void beepArmedAlert();

    void stop();
};

#endif