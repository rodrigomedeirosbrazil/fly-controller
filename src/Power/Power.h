#ifndef POWER_H
#define POWER_H

class Power {
public:
    Power(int escMinPwm, int escMaxPwm);
    void update(float throttlePercent, float batteryPercent, float escTemp, float motorTemp, float current);
    int getPwm() const;

private:
    float calcBatteryLimit(float batteryPercent) const;
    float calcEscTempLimit(float escTemp) const;
    float calcMotorTempLimit(float motorTemp) const;
    float calcCurrentLimit(float current) const;

    int escMinPwm;
    int escMaxPwm;
    float lastThrottlePercent;
    float lastBatteryPercent;
    float lastEscTemp;
    float lastMotorTemp;
    float lastCurrent;
    int pwm;
};

#endif // POWER_H