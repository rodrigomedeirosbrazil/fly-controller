#ifndef REMOTE_PINS_H
#define REMOTE_PINS_H

// ESP32-C3 Supermini pin assignments for the remote throttle.
#define HALL_PIN        0   // ADC1_CH0, analogRead
#define BUTTON_PIN      5   // INPUT_PULLUP
#define BUZZER_PIN      6   // LEDC PWM
#define LED_RED_PIN     7   // armed
#define LED_GREEN_PIN   10  // disarmed

#endif // REMOTE_PINS_H
