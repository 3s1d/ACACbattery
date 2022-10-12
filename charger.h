#ifndef __CHARGER_H
#define __CHARGER_H

#include <Arduino.h> 

class Charger
{
public: 
  /* SPI */  
  const uint32_t cs = 18;          //active high, due to opto coupler
  const uint32_t sclk = 19;
  const uint32_t din = 5;
  const uint16_t maxDAC = 860;   //vref is fead via voltage halfer (resistor) from charger, effectivly canceling out 2x gain of tlc5615

  const uint32_t powerPin = 4;

  const float minChrgPwr_w = -66.0f;
  const float maxChrgPwr_w = -1560.0f;
  const float wattPerStep = (maxChrgPwr_w-minChrgPwr_w) / maxDAC;
  const float powerMarginW = 2.0f*minChrgPwr_w;

  const float ki = 0.3f;

private:
  float state = 0.0f;
  bool _active = false;

  uint32_t suspendCtlforCycles = 0;       //note: siliently assuming 1Hz call rate. Charger takes 4sec to warm up

  void tlc5615(uint16_t dac);

public:
  const bool &active;

  Charger() : active(_active) { }

  float getMaxPower_w(void) { return (active == false) ? 0.0f : (wattPerStep * state + minChrgPwr_w); }

  void setup(void);

  void setMaxPower_w(float watt);
  void off(void);
};

extern Charger charger;

#endif
