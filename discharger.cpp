#include "esp32-hal-gpio.h"
#include "sun2000softLimiterEmu.h"

#include "discharger.h"

//note: controlling sun2k using a PI controller
void Discharger::setMaxPower_w(float watt)
{
  Serial.print("Discharger: ");
  Serial.print(watt);
  Serial.println("W required");

  /* perfect balance */
  if(watt <= powerMarginW and watt >= 0.0f)
    return;

  /* adjust power */
  const float dPwr = watt - powerMarginW/2.0f;          //targer is powerMarginW/2.0f (import)
  const int16_t dSteps = dPwr / wattPerStep * ki;
  float nextState = state+dSteps;
  if(nextState > limit)
    nextState = limit;
  else if(nextState < 0.0f)
    nextState = 0.0f;
  dacWrite(dacCh, round(nextState));
  
  state = nextState;

  /* control final output */
  _active = state > 0.5f or watt >= 0;
  softLimiterEmu.set(active ? 1000.0f : -1000.0f);

#if 0
  Serial.print("is:");
  Serial.println(watt);
  Serial.print("dSteps:");
  Serial.println(dSteps);
  Serial.print("state:");
  Serial.println(state);
  Serial.print("act:");
  Serial.println(active*100);
#endif
}

void Discharger::off(void)
{
  state = 0.0f;
  dacWrite(dacCh, round(state));
  _active = false;
  softLimiterEmu.set(-1000.0f);
}

void Discharger::setup(void)
{
  state = 0;
  _active = 0;
  dacWrite(dacCh, state);

  /* limiter */
  softLimiterEmu.setup();
  softLimiterEmu.set(-1000.0f);   //disable output
}

Discharger discharger;