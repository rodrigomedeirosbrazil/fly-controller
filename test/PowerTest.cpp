#include <iostream>
#include <cassert>
#include <algorithm>
using namespace std;

// Mocked constants
#define ESC_MIN_PWM 1000
#define ESC_MAX_PWM 2000
#define BATTERY_MIN_VOLTAGE 462
#define BATTERY_MAX_VOLTAGE 588
#define MOTOR_MAX_TEMP 80

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

        return map(
            batteryPercentage,
            0,
            10,
            0,
            100
        );
    }

    unsigned int calcMotorTempLimit() {
        double readedMotorTemp = motorTemp.getTemperature();
        if (readedMotorTemp < MOTOR_MAX_TEMP - 10) {
            return 100;
        }
        return map(
            (double)readedMotorTemp,
            (double)(MOTOR_MAX_TEMP - 10),
            (double)MOTOR_MAX_TEMP,
            100,
            0
        );
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
    // 5% battery: x = BATTERY_MIN_VOLTAGE + 0.05 * (BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE)
    p.canbus.voltage = (BATTERY_MIN_VOLTAGE + (BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE) * 0.05) + 1; // 5% value
    assert(p.calcBatteryLimit() == 50);
    std::cout << "test_calcBatteryLimit passed\n";
}

void test_calcMotorTempLimit() {
    Power p;
    p.motorTemp.temp = 60;
    assert(p.calcMotorTempLimit() == 100);
    p.motorTemp.temp = 80;
    assert(p.calcMotorTempLimit() == 0);
    p.motorTemp.temp = 75;
    int limit = p.calcMotorTempLimit();
    assert(limit < 100 && limit > 0);
    std::cout << "test_calcMotorTempLimit passed\n";
}

void test_calcPower() {
    Power p;
    p.canbus.ready = true;
    p.canbus.voltage = 4200;
    p.motorTemp.temp = 25;
    assert(p.calcPower() == 100);
    p.canbus.voltage = 3300;
    assert(p.calcPower() == 100);
    p.canbus.voltage = 4200;
    p.motorTemp.temp = 80;
    assert(p.calcPower() == 0);
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