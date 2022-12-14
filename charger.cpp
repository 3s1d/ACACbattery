#include "HardwareSerial.h"
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
void Charger::setMaxPower_w(float watt, bool boostable)
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
      offDelay = 4;
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

  /* booster */
  if(boostable == false and digitalRead(boostPin) == HIGH)
  {
    digitalWrite(boostPin, LOW);
    Serial.println("Boost off (ext)\n");
  }
  else if(boostable == true and watt < boostPwr_w and state >= round(maxDAC) and digitalRead(boostPin) == LOW)
  {
    digitalWrite(boostPin, HIGH);
    Serial.println("Boost on!\n");
    return;
  }

  const float dPwr = watt - powerMarginW/2.0f;          //targer is powerMarginW/2.0f (import)
  float nextState; 

  /* adjust power */
  if(digitalRead(nFullyCharged) == LOW)
  {
    /* fully charged indicator on -> will not consume any power -> control P only as I will max out pwr consumption */
    const float absSteps = round(dPwr / wattPerStep * kp);
    nextState = absSteps;
  }
  else
  {
    /* pure I control */
    const float dSteps = round(dPwr / wattPerStep * ki);
    nextState = state+dSteps;
  }

  if(nextState > maxDAC)
    nextState = maxDAC;
  else if(nextState < 0.0f)
    nextState = 0.0f;
  tlc5615(round(nextState));
  state = nextState;

  if(digitalRead(boostPin) == HIGH and state <= ((float)maxDAC)*0.5f)
  {
    digitalWrite(boostPin, LOW);
    Serial.println("Boost off (pwr)\n");

    if(watt < -boostPwr_w)
      offDelay = 5;   //yield extra second just in case of import less than booster consumption...
  }

  /* control final output */
  bool nextActive = state > 0.0f or watt <= 0.0f;
  if(nextActive == true)
  {
    _active = nextActive;
    offDelay = 4;
  }
  else if(offDelay > 0)
  {
    Serial.print("Chg off delay: ");
    Serial.println(offDelay);
    offDelay--;
  }
  else 
  {
    _active = nextActive;
  }
  digitalWrite(powerPin, active ? HIGH : LOW);

}

float Charger::getMaxPower_w(void) 
{ 
  /* in case of non active, check pin manually in case some emergency stuff is going on */
  if(active == false)
  {
    //note: inverted values for emergency charging 
    if(digitalRead(powerPin))
      return -( (wattPerStep * state + minChrgPwr_w) + (!!digitalRead(boostPin) * boostPwr_w) );
    else
      return 0;
  }

  /* active stuff */
  return (wattPerStep * state + minChrgPwr_w) + (!!digitalRead(boostPin) * boostPwr_w); 
}

void Charger::emergencyCharge(bool en, bool heating)
{
  /* don't interfere w/ normal operation */
  if(active)
    return;

  /* start minimal charging */
  state = (heating and en) ? (maxDAC/3.0f) : 0.0f;
  tlc5615(round(state));
  digitalWrite(powerPin, en);

  //note: we can not set DAC if no power
  if(en)
  {
    delay(200);             //100ms tested
    tlc5615(round(state));
  }
}

void Charger::off(void)
{
  Serial.println("CHG OFFFFFF\n");
  
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
  pinMode(boostPin, OUTPUT);
  digitalWrite(boostPin, LOW);

  pinMode(nFullyCharged, INPUT_PULLUP);
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
  tlc5615(round(state));
}

Charger charger;