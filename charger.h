#include "esp32-hal-gpio.h"
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
  const uint32_t boostPin = 27;
  const uint32_t nFullyCharged = 23;

  const float minChrgPwr_w = -67.0f;
  const float maxChrgPwr_w = -1560.0f;
  const float wattPerStep = (maxChrgPwr_w-minChrgPwr_w) / maxDAC;
  const float powerMarginW = 2.0f*minChrgPwr_w;

  const float boostPwr_w = -780.0f;

  const float ki = 0.25f;   //0.3f
  const float kp = 0.7f;

private:
  float state = 0.0f;
  bool _active = false;
  int16_t offDelay = 0;

  uint32_t suspendCtlforCycles = 0;       //note: siliently assuming 1Hz call rate. Charger takes 4sec to warm up

  void tlc5615(uint16_t dac);

public:
  const bool &active;

  Charger() : active(_active) { }

  float getMaxPower_w(void);
  void setup(void);

  void setMaxPower_w(float watt, bool boostable);
  void emergencyCharge(bool en, bool heating = false);
  void off(void);
};

extern Charger charger;

#endif
