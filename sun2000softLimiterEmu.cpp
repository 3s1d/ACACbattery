#include "Sun2000softLimiterEmu.h"

/* Arduino has a very poor interface, not providing any parameter like void *arg for callbacks -> hopefully nobody attaches more then one SUN2000 to this hardware */
//note: FunctionalInterrupt could be used fir IRQ. However, for the timer no such thing does exists.
Sun2000softLimiterEmu softLimiterEmu = Sun2000softLimiterEmu(SUN2000_LIMITER, SUN2000_ACLINE);

/* poor mans stupid arg'less wrappers */
void IRAM_ATTR onAcWrapper(void)
{
  softLimiterEmu.onAc();
}

void IRAM_ATTR onTimerWrapper(void)
{
  softLimiterEmu.onTimer();
}

void IRAM_ATTR Sun2000softLimiterEmu::onAc(void)
{
 
  portENTER_CRITICAL(&mux);
  pulseWidth_us = nextPulseWidth_us;
  pulseShift_us = nextSignShift_us;
  portEXIT_CRITICAL(&mux);

  if(state == 0 and pulseWidth_us > 0)
  {
    detachInterrupt(digitalPinToInterrupt(aclinePin));

    state = 1;
    timerWrite(timer, 0);
    timerAlarmWrite(timer, acDelay - pulseWidth_us + pulseShift_us, false);      //pre pulse
    timerAlarmEnable(timer);
  }
  else
  {
    state = 0;      //state correction...
    digitalWrite(limiterPin, LOW);
  }
 
}

void IRAM_ATTR Sun2000softLimiterEmu::onTimer(void)
{
  switch(state)
  {
    case 1:                                     //activate pulse
      digitalWrite(limiterPin, HIGH);
      state = 2;
      timerAlarmWrite(timer, acDelay + pulseShift_us, false);
      break;
    case 2:                                     //deactivate pulse
      digitalWrite(limiterPin, LOW);
      state = 3;
      timerAlarmWrite(timer, 18000, false);     //somewhere in the back of the wave
      break;
   case 3:                                      //done, reactivate irq
   default:
      state = 0;
      timerAlarmDisable(timer);
      attachInterrupt(digitalPinToInterrupt(aclinePin), onAcWrapper, RISING);
  }
}


void Sun2000softLimiterEmu::setup(int32_t timerNum)
{
  timer = timerBegin(timerNum, 80, true);
  timerAttachInterrupt(timer, &onTimerWrapper, true);
  
  pinMode(aclinePin, INPUT_PULLDOWN);
  pinMode(limiterPin, OUTPUT);
  state = 0;
  attachInterrupt(digitalPinToInterrupt(aclinePin), onAcWrapper, RISING);

  set(0.0f);
}

void Sun2000softLimiterEmu::set(float watt)
{
  watt -= exportErrorMargin_watt;
  if(watt > limit_watt)
    watt = limit_watt;

  //note: limiting export here as well in order not to hit timing limits. anyway, export is uninteresting...
  if(watt < -limit_watt)
    watt = -limit_watt;
 
  if(watt * inclination + offset > 0.0f)
  {
    portENTER_CRITICAL(&mux);
    nextPulseWidth_us = watt * inclination + offset;
    nextSignShift_us = signShift * !!nonInvert;
    portEXIT_CRITICAL(&mux);
  }
  else if(abs(watt) * inclination - offset > 0.0f)
  {
    portENTER_CRITICAL(&mux);
    nextPulseWidth_us = abs(watt) * inclination - offset;
    nextSignShift_us = signShift * !nonInvert;
    portEXIT_CRITICAL(&mux);
  }
  else
  {
    Serial.print("Something is wrong: ");
    Serial.println(watt);
    nextPulseWidth_us = 0;
  }
}
