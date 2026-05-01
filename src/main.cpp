#include <Arduino.h>
#include <ESP32Servo.h>
#include <driver/twai.h>
#include <string.h>

#define BUZZER_PIN       6
#define ESC_PIN          7
#define CAN_TX_PIN       2
#define CAN_RX_PIN       3
#define ESC_MIN_PWM      1100
#define PRINT_INTERVAL_MS 1000  // cada ID imprime no máximo 1x por segundo

Servo esc;

struct CachedFrame {
    uint32_t      id;
    unsigned long lastPrint;
    bool          used;
};
static CachedFrame cache[64];

static bool shouldPrint(const twai_message_t& msg)
{
    unsigned long now = millis();
    for (int i = 0; i < 64; i++) {
        if (!cache[i].used) {
            cache[i].used      = true;
            cache[i].id        = msg.identifier;
            cache[i].lastPrint = now;
            return true;
        }
        if (cache[i].id == msg.identifier) {
            if (now - cache[i].lastPrint >= PRINT_INTERVAL_MS) {
                cache[i].lastPrint = now;
                return true;
            }
            return false;
        }
    }
    return true;
}

static void printFrame(const twai_message_t& msg)
{
    Serial.printf("id=0x%08X dlc=%d |", msg.identifier, msg.data_length_code);
    for (uint8_t i = 0; i < msg.data_length_code; i++) {
        Serial.printf(" %02X", msg.data[i]);
    }
    Serial.println();
}

void setup()
{
    memset(cache, 0, sizeof(cache));

    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);

    Serial.begin(115200);
    Serial.println("[CanMonitor] starting");

    esc.attach(ESC_PIN);
    esc.writeMicroseconds(ESC_MIN_PWM);

    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
        (gpio_num_t)CAN_TX_PIN,
        (gpio_num_t)CAN_RX_PIN,
        TWAI_MODE_NORMAL
    );
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_1MBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) {
        Serial.println("[CanMonitor] ERROR: twai_driver_install failed");
        return;
    }
    if (twai_start() != ESP_OK) {
        Serial.println("[CanMonitor] ERROR: twai_start failed");
        return;
    }
    Serial.println("[CanMonitor] CAN ready @ 1 Mbps");
}

void loop()
{
    twai_message_t msg;
    while (twai_receive(&msg, 0) == ESP_OK) {
        if (shouldPrint(msg)) {
            printFrame(msg);
        }
    }
}
