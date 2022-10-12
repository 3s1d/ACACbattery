# AC AC Battery

Working... Testing.
Using an ESP32 with Arduino.

## Discharging

Sun2000 GTIL2 gets grid import/export power from a SDM630 via RS485->MQTT. Internal limiter gets emulated via single GPIO pin connected to single pin of limiter (leave the other floating). Ground gets connected to pin 5 of remote connector. Actual power output is controlled by the inverter itself.

Phase zero crossing detector is required to sync power emulation. In case of wrong sign, change half-wave by 'void Sun2000softLimiterEmu::setPhase()' or swap AC pins.
![Zero crossing detector](https://raw.githubusercontent.com/3s1d/ACACbattery/master/zeroCrossing.png)

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

Additionally Sun2000's RS232 is used to collect some more information. This is based upon [GFSunInverter](https://github.com/BlackSmith/GFSunInverter) by BlackSmith.

For power control using the internal limiter an update rate of at least 10Hz+ is required. Otherwise the system will oscillate. If not obtainable (as it is with my SDM630+mqtt system) a fallback is to use the analog 0..1.6V input for power control. This is done via a voltage divider plus op-amp to reduce the impedance of the ESP's DAC. Resulting in a gain of 0.5. The limiter is still used to fully turn of the inverter by 'emulating' negative power.

## Charging
 
Charging is handle via an adjustable charger. Something like this: https://de.aliexpress.com/item/33012684821.html?spm=a2g0o.order_list.0.0.55815c5fdZnjNI&gatewayAdapt=glo2deu

The potentiometer for power adjustments gets replaced by a TLC5615. The positive pin gets feed into Vref after passing through a voltage divider of 0.5 (to compensate the TLC5615's internal gain of 2). Vout gets connected to the old wiper pin of the potentiometer. Plus one relay to emulate the power button. The setup gets isolated by ADUM1200 + opto-coupler (for CS) from the discharging unit. 

Please note: Due to a lot of EMI use decoupling capacitors everywhere.
