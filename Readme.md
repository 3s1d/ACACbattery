# AC AC Battery

Work in Progress !!!
Using an ESP32 with Arduino.

## Discharging

Sun2000 GTIL2 gets grid import/export power from a SDM630 via RS485->MQTT. Internal limiter gets emulated via single GPIO pin connected to single pin of limiter (leave the other floating). Ground gets connected to pin 5 of remote connector. Actual power output is controlled by the inverter itself.

Phase zero crossing detector is required to sync power emulation. In case of wrong sign, change half-wave by 'void Sun2000softLimiterEmu::setPhase()' or swap AC pins.
![Zero crossing detector](http://url/to/zeroCrossing.png)

Parameters:
```
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
```
Those values may be subject to change for your setup. Please note the export margin in Watts. The inverter will display 20 Watts less than the provided value.

## Charging
 
todo
