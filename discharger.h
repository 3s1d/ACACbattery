#ifndef __DISCHARGER_H
#define __DISCHARGER_H


class Discharger
{
public:
  const uint32_t dacCh = 26;          //DAC2
  const uint8_t limit = 240;          //approx 1750W    1:2 divider + opamp
  const float wattPerStep = 7.875f;
  const float powerMarginW = 4.0f*wattPerStep;
  const float ki = 0.45f;           //0.5f;

private:

  float state = 0.0f;
  bool _active = false;
  int16_t offDelay = 0;

public:
  const bool &active;

  Discharger() : active(_active) { }

  void setup(void);

  void setMaxPower_w(float watt);  
  float getMaxPower_w(void) { return (active == false) ? 0.0f : (wattPerStep * state); }
  void off(void);
};

extern Discharger discharger;

#endif