#include "esp32-hal.h"
#include "esp32-hal-gpio.h"
#include "charger.h"


void Charger::tlc5615(uint16_t dac)
{
  /* cs */
  //note: active high
  digitalWrite(sclk, LOW);
  digitalWrite(cs, HIGH);  
  delay(10);

  /* limit */
  if(dac > maxDAC)
    dac = maxDAC;

  /* shift to correct position */
  dac = (dac & 0x3FF) << 2;

  /* pulse out */
  for(int16_t i=15; i>=0; i--)
  {
    digitalWrite(din, (dac & (1<<i)) ? HIGH : LOW);
    delayMicroseconds(125);
    digitalWrite(sclk, HIGH);
    delayMicroseconds(105);
    digitalWrite(sclk, LOW);
    delayMicroseconds(20);
  }

  digitalWrite(cs, LOW);
  delay(2); 
#if 0
  Serial.print("dac: ");
  Serial.println(dac >> 2);
#endif
}

//note: working w/ neg/export values here
void Charger::setMaxPower_w(float watt)
{
  Serial.print("Charger max: ");
  Serial.print(watt);
  Serial.println("W available");

  if(active == false)
  {
    /* power up */
    if(watt < powerMarginW)
    {
      state = 0.0f;
      tlc5615(round(state));
      digitalWrite(powerPin, HIGH);
      _active = true;
      suspendCtlforCycles = 5;      //charger warm up
    }      
    return;
  }

  /* delayed control start */
  if(suspendCtlforCycles > 0)
  {
    tlc5615(round(state));
    suspendCtlforCycles--;
    return;
  }

   /* perfect balance */
  if(watt > powerMarginW and watt <= 0.0f)
    return;

  /* adjust power */
  const float dPwr = watt - powerMarginW/2.0f;          //targer is powerMarginW/2.0f (import)
  const float dSteps = round(dPwr / wattPerStep * ki);
  float nextState = state+dSteps;
  if(nextState > maxDAC)
    nextState = maxDAC;
  else if(nextState < 0.0f)
    nextState = 0.0f;
  tlc5615(round(nextState));
  state = nextState;

  /* control final output */
  bool nextActive = state > 0.0f or watt <= 0.0f;
  if(nextActive == true)
  {
    _active = nextActive;
    if(state > 10.0f)
      offDelay = 2;
  }
  else if(offDelay > 0)
  {
    offDelay--;
  }
  else 
  {
    _active = nextActive;
  }
  digitalWrite(powerPin, active ? HIGH : LOW);

}

void Charger::off(void)
{
  /* reduce power */ 
  state = 0.0f;
  tlc5615(round(state));

  /* let sattle a little */
  delay(250);

  /* turn off */
  digitalWrite(powerPin, LOW);
  _active = false;
}

void Charger::setup(void)
{
  pinMode(powerPin, OUTPUT);
  digitalWrite(powerPin, LOW);
  _active = 0;

  pinMode(cs, OUTPUT);
  digitalWrite(cs, LOW);
  delay(1);
  pinMode(din, OUTPUT);
  digitalWrite(din, LOW);
  pinMode(sclk, OUTPUT);
  digitalWrite(sclk, LOW);
  delay(50);

  state = 0.0f;
  tlc5615(round(state));
}

Charger charger;