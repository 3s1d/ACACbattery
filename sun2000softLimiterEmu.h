#ifndef SUN2000SOFTLIMITER_H
#define SUN2000SOFTLIMITER_H

#include <Arduino.h>

#define SUN2000_LIMITER      13
#define SUN2000_ACLINE       21

class Sun2000softLimiterEmu
{
public:
/*
 * Values determined empirically. 
 * 1st: acDealy. Look for peak wattage for a fixed pulse width (should be the same for everybody)
 * 2nd: inclination/offset. Map pulse width vs displayed watts and apply linear fitting to the curve.
 */
  const int32_t acDelay = 4250;           //unit: us
  const int32_t signShift = 10000;        //unit: us
  const float inclination = 0.7675f;      //unit: us/w
  const int16_t offset = 15;              //unit: us 
  
  const float limit_watt = 1700.0f;       //GTIL2 continuous limit is 1800W, not going to max here in order not to damage the hardware
  const float exportErrorMargin_watt = 20.0f;

  const int32_t limiterPin;
  const int32_t aclinePin;

private:
  int32_t pulseWidth_us = 0;
  int32_t pulseShift_us = 0;
  
  int32_t nextPulseWidth_us = 0;
  int32_t nextSignShift_us = 0;
  
  bool nonInvert = true;
  portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
  hw_timer_t *timer = NULL;
  int state = 0;


  void IRAM_ATTR onAc(void);
  void IRAM_ATTR onTimer(void);

  friend void IRAM_ATTR onAcWrapper(void);
  friend void IRAM_ATTR onTimerWrapper(void);

public:
  Sun2000softLimiterEmu(int32_t limiterPin, int32_t aclinePin) : limiterPin(limiterPin), aclinePin(aclinePin) { }

  void setup(int32_t timerNum = 0);

  /* use to invert sign of idaplyed current */
  /* or simply reverse ac zero crossing lines */
  void setPhase(bool secondSpikePos) { nonInvert = secondSpikePos; }
  
  void set(float watt);
  void off(void);
  
};

//SIGNLE instance only due to no args for static callbacks...
extern Sun2000softLimiterEmu softLimiterEmu;

#endif
