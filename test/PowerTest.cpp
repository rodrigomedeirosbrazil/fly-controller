#include <iostream>
#include <cassert>
#include <algorithm>
using namespace std;

// Mocked constants
#define ESC_MIN_PWM 1000
#define ESC_MAX_PWM 2000
#define BATTERY_MIN_VOLTAGE 462
#define BATTERY_MAX_VOLTAGE 588
#define MOTOR_MAX_TEMP 60

// Mocked dependencies
struct MockCanbus {
    bool ready = true;
    int voltage = 4200;
    bool isReady() { return ready; }
    int getMiliVoltage() { return voltage; }
};

struct MockThrottle {
    unsigned int percent = 100;
    unsigned int getThrottlePercentage() { return percent; }
};

struct MockMotorTemp {
    double temp = 25.0;
    double getTemperature() { return temp; }
};

// Power class (methods copied and adapted)
class Power {
public:
    unsigned long lastPowerCalculationTime;
    unsigned int pwm;
    unsigned int power;
    MockCanbus canbus;
    MockThrottle throttle;
    MockMotorTemp motorTemp;

    Power() {
        lastPowerCalculationTime = 0;
        pwm = ESC_MIN_PWM;
        power = 100;
    }

    unsigned int getPwm() {
        unsigned int effectivePercent = (throttle.getThrottlePercentage() * getPower()) / 100;
        return map(
            effectivePercent,
            0,
            100,
            ESC_MIN_PWM,
            ESC_MAX_PWM
        );
    }

    unsigned int getPower() {
        // For test, always recalc
        power = calcPower();
        return power;
    }

    unsigned int calcPower() {
        unsigned int batteryLimit = calcBatteryLimit();
        unsigned int motorTempLimit = calcMotorTempLimit();
        return std::min(batteryLimit, motorTempLimit);
    }

    unsigned int calcBatteryLimit() {
        if (!canbus.isReady()) {
            return 0;
        }
        int batteryMilliVolts = canbus.getMiliVoltage();
        int batteryPercentage = map(
            batteryMilliVolts,
            BATTERY_MIN_VOLTAGE,
            BATTERY_MAX_VOLTAGE,
            0,
            100
        );

        if (batteryPercentage > 10) {
            return 100;
        }

        int mapped = map(
            batteryPercentage,
            0,
            10,
            0,
            100
        );

        return constrain(mapped, 0, 100);
    }

    unsigned int calcMotorTempLimit() {
        double readedMotorTemp = motorTemp.getTemperature();
        if (readedMotorTemp < MOTOR_MAX_TEMP - 10) {
            return 100;
        }
        int mapped = map(
            (double)readedMotorTemp,
            (double)(MOTOR_MAX_TEMP - 10),
            (double)MOTOR_MAX_TEMP,
            100,
            0
        );
        // Garante que o valor fique entre 0 e 100
        return constrain(mapped, 0, 100);
    }

    // Simple map function (like Arduino's)
    static int map(int x, int in_min, int in_max, int out_min, int out_max) {
        if (in_max == in_min) return out_min;
        return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
    }
    static int map(double x, double in_min, double in_max, int out_min, int out_max) {
        if (in_max == in_min) return out_min;
        return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
    }

    static int constrain(int amt, int low, int high) {
        return (amt < low) ? low : ((amt > high) ? high : amt);
    }
};

// Test functions
void test_calcBatteryLimit() {
    Power p;
    p.canbus.ready = false;
    assert(p.calcBatteryLimit() == 0);
    p.canbus.ready = true;
    p.canbus.voltage = BATTERY_MAX_VOLTAGE; // 588 (58.8V)
    assert(p.calcBatteryLimit() == 100);
    p.canbus.voltage = BATTERY_MIN_VOLTAGE; // 462 (46.2V)
    assert(p.calcBatteryLimit() == 0);
    p.canbus.voltage = (BATTERY_MIN_VOLTAGE + (BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE) * 0.05) + 1; // 5% value
    assert(p.calcBatteryLimit() == 50);
     p.canbus.voltage = BATTERY_MIN_VOLTAGE - 100;
    assert(p.calcBatteryLimit() == 0);
    std::cout << "test_calcBatteryLimit passed\n";
}

void test_calcMotorTempLimit() {
    Power p;
    p.motorTemp.temp = 60;
    assert(p.calcMotorTempLimit() == 0);
    p.motorTemp.temp = 50;
    assert(p.calcMotorTempLimit() == 100);
    p.motorTemp.temp = 55;
    assert(p.calcMotorTempLimit() == 50);
    p.motorTemp.temp = MOTOR_MAX_TEMP + 5;
    assert(p.calcMotorTempLimit() == 0);
    std::cout << "test_calcMotorTempLimit passed\n";
}

void test_calcPower() {
    Power p;
    // Caso: tudo normal, sem limitação
    p.canbus.ready = true;
    p.canbus.voltage = BATTERY_MAX_VOLTAGE;
    p.motorTemp.temp = MOTOR_MAX_TEMP - 15;
    assert(p.calcPower() == 100 && "Power should be 100 when all limits are OK");

    // Caso: limitação por bateria (5%)
    int five_percent_voltage = (BATTERY_MIN_VOLTAGE + (BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE) * 0.05) + 1;
    p.canbus.voltage = five_percent_voltage;
    p.motorTemp.temp = MOTOR_MAX_TEMP - 15;
    assert(p.calcPower() == 50 && "Power should be 50 when battery is at 5%");

    // Caso: limitação por temperatura (meio do range)
    p.canbus.voltage = BATTERY_MAX_VOLTAGE;
    p.motorTemp.temp = MOTOR_MAX_TEMP - 5;
    assert(p.calcPower() == 50 && "Power should be 50 when motor temp is at midpoint");

    // Caso: limitação por ambos (o menor prevalece)
    p.canbus.voltage = five_percent_voltage;
    p.motorTemp.temp = MOTOR_MAX_TEMP - 5;
    assert(p.calcPower() == 50 && "Power should be 50 when both limits are 50");

    // Caso: limitação total por temperatura (acima do máximo)
    p.canbus.voltage = BATTERY_MAX_VOLTAGE;
    p.motorTemp.temp = MOTOR_MAX_TEMP + 5;
    assert(p.calcPower() == 0 && "Power should be 0 when temp is above max");

    // Caso: limitação total por bateria (abaixo do mínimo)
    p.canbus.ready = true;
    p.canbus.voltage = BATTERY_MIN_VOLTAGE;
    p.motorTemp.temp = MOTOR_MAX_TEMP - 15;
    assert(p.calcPower() == 0 && "Power should be 0 when battery is at min");

    // Caso: CAN bus não pronto
    p.canbus.ready = false;
    p.motorTemp.temp = MOTOR_MAX_TEMP - 15;
    assert(p.calcPower() == 0 && "Power should be 0 when CAN bus is not ready");

    std::cout << "test_calcPower passed\n";
}

void test_getPwm() {
    Power p;
    p.throttle.percent = 100;
    p.canbus.ready = true;
    p.canbus.voltage = 4200;
    p.motorTemp.temp = 25;
    assert(p.getPwm() == ESC_MAX_PWM);
    p.throttle.percent = 50;
    assert(p.getPwm() == (ESC_MIN_PWM + (ESC_MAX_PWM - ESC_MIN_PWM) / 2));
    p.canbus.voltage = 3300;
    assert(p.getPwm() == (ESC_MIN_PWM + (ESC_MAX_PWM - ESC_MIN_PWM) / 2));
    std::cout << "test_getPwm passed\n";
}

int main() {
    test_calcBatteryLimit();
    test_calcMotorTempLimit();
    test_calcPower();
    test_getPwm();
    std::cout << "All tests passed!\n";
    return 0;
}