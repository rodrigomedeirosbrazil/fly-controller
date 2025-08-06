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
    int deciVoltage = 5000;
    bool isReady() { return ready; }
    int getDeciVoltage() { return deciVoltage; }
};

struct MockThrottle {
    unsigned int raw = 1000;
    unsigned int min = 1000;
    unsigned int max = 2000;
    unsigned int getThrottleRaw() { return raw; }
    int getThrottlePinMin() { return min; }
    int getThrottlePinMax() { return max; }
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
    unsigned int batteryPowerFloor;
    MockCanbus canbus;
    MockThrottle throttle;
    MockMotorTemp motorTemp;

    Power() {
        lastPowerCalculationTime = 0;
        pwm = ESC_MIN_PWM;
        power = 100;
        batteryPowerFloor = 100;
    }

    unsigned int getPwm() {
        unsigned int throttleRaw = throttle.getThrottleRaw();
        unsigned int powerLimit = getPower();
        int throttleMin = throttle.getThrottlePinMin();
        int throttleMax = throttle.getThrottlePinMax();
        int allowedMax = throttleMin + ((throttleMax - throttleMin) * powerLimit) / 100;
        unsigned int effectiveRaw = constrain(throttleRaw, throttleMin, allowedMax);
        return map(
            effectiveRaw,
            throttleMin,
            throttleMax,
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
        const unsigned int STEP_DECREASE = 5;
        if (canbus.getDeciVoltage() > BATTERY_MIN_VOLTAGE) {
            return batteryPowerFloor;
        }
        if (batteryPowerFloor < STEP_DECREASE) {
            batteryPowerFloor = 0;
            return batteryPowerFloor;
        }
        batteryPowerFloor = batteryPowerFloor - STEP_DECREASE;
        return batteryPowerFloor;
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

    void setBatteryPowerFloor(unsigned int value) { batteryPowerFloor = value; }
};

// Test functions
void test_calcBatteryLimit() {
    Power p;
    p.canbus.ready = false;
    assert(p.calcBatteryLimit() == 0);
    p.canbus.ready = true;
    p.canbus.deciVoltage = BATTERY_MAX_VOLTAGE; // Acima do mínimo
    p.setBatteryPowerFloor(80);
    std::cout << "[DEBUG] batteryPowerFloor=" << p.batteryPowerFloor << ", calcBatteryLimit()=" << p.calcBatteryLimit() << std::endl;
    assert(p.calcBatteryLimit() == 80); // Não altera
    assert(p.batteryPowerFloor == 80);

    // Agora simula queda de voltagem
    p.canbus.deciVoltage = BATTERY_MIN_VOLTAGE - 1; // Abaixo do mínimo
    p.setBatteryPowerFloor(15);
    assert(p.calcBatteryLimit() == 10); // 15-5
    assert(p.batteryPowerFloor == 10);
    assert(p.calcBatteryLimit() == 5);  // 10-5
    assert(p.batteryPowerFloor == 5);
    assert(p.calcBatteryLimit() == 0);  // 5-5
    assert(p.batteryPowerFloor == 0);
    assert(p.calcBatteryLimit() == 0);  // já está zerado
    assert(p.batteryPowerFloor == 0);
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
    p.canbus.deciVoltage = BATTERY_MAX_VOLTAGE;
    p.motorTemp.temp = MOTOR_MAX_TEMP - 15;
    p.setBatteryPowerFloor(100);
    assert(p.calcPower() == 100 && "Power should be 100 when all limits are OK");

    // Caso: limitação por bateria (abaixo do mínimo, stepwise)
    p.canbus.deciVoltage = BATTERY_MIN_VOLTAGE - 1;
    p.setBatteryPowerFloor(15);
    p.motorTemp.temp = MOTOR_MAX_TEMP - 15;
    assert(p.calcPower() == 10 && "Power should be 10 after one step");
    assert(p.calcPower() == 5 && "Power should be 5 after two steps");
    assert(p.calcPower() == 0 && "Power should be 0 after three steps");

    // Caso: limitação por temperatura (meio do range)
    p.canbus.deciVoltage = BATTERY_MAX_VOLTAGE;
    p.setBatteryPowerFloor(100);
    p.motorTemp.temp = MOTOR_MAX_TEMP - 5;
    assert(p.calcPower() == 50 && "Power should be 50 when motor temp is at midpoint");

    // Caso: limitação por ambos (o menor prevalece)
    p.canbus.deciVoltage = BATTERY_MIN_VOLTAGE - 1;
    p.setBatteryPowerFloor(15);
    p.motorTemp.temp = MOTOR_MAX_TEMP - 5;
    assert(p.calcPower() == 10 && "Power should be 10 (battery limit < temp limit)");
    p.setBatteryPowerFloor(5);
    assert(p.calcPower() == 0 && "Power should be 0 (battery limit < temp limit)");

    // Caso: limitação total por temperatura (acima do máximo)
    p.canbus.deciVoltage = BATTERY_MAX_VOLTAGE;
    p.setBatteryPowerFloor(100);
    p.motorTemp.temp = MOTOR_MAX_TEMP + 5;
    assert(p.calcPower() == 0 && "Power should be 0 when temp is above max");

    // Caso: limitação total por bateria (abaixo do mínimo)
    p.canbus.ready = true;
    p.canbus.deciVoltage = BATTERY_MIN_VOLTAGE;
    p.setBatteryPowerFloor(0);
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
    p.throttle.min = 1000;
    p.throttle.max = 2000;
    p.throttle.raw = 2000;
    p.canbus.ready = true;
    p.canbus.deciVoltage = 4200;
    p.motorTemp.temp = 25;
    assert(p.getPwm() == ESC_MAX_PWM);
    p.throttle.raw = 1500;
    assert(p.getPwm() == (ESC_MIN_PWM + (ESC_MAX_PWM - ESC_MIN_PWM) / 2));
    p.canbus.deciVoltage = 3300;
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