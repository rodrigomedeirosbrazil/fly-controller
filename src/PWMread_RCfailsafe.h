#ifndef PWMread_RCfailsafe_h
#define PWMread_RCfailsafe_h

boolean FAILSAFE(int CH);
float calibrate(float Rx, int Min, int Mid, int Max);
void setup_pwmRead();
boolean RC_avail();
float RC_decode(int CH);

#endif
