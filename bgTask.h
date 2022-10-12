#ifndef __BG_TASK_H
#define __BG_TASK_H

#include <OneWire.h>
#include <DallasTemperature.h>

#include "GfSun2000.h"

extern "C" void dataHandler(GfSun2000Data data);

class Background
{
  public:
    typedef struct
    {
        float vbat_v;
        float avgPwr_w;
        float energyToday_kwh;
        uint32_t tstamp;
    } sun2k_info_t;

    typedef struct
    {
      float temp;
      uint32_t tstamp;
    } temp_info_t;

  const uint32_t oneWirePin = 32;

private:
  GfSun2000 GF;
  OneWire oneWire;
  DallasTemperature tempsens;
  temp_info_t tempInfo = { temp : DEVICE_DISCONNECTED_C };
  uint32_t tempErrorDelay = 0;

  TaskHandle_t sun2000_task;
  portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
  sun2k_info_t s2kInfo;

  void task(void);

  friend void bg_task(void *arg);
  friend void dataHandler(GfSun2000Data data);

public:
    void start(void);
    sun2k_info_t getS2kInfo(void);
    temp_info_t getTempInfo(void);
};

extern Background bg;

#endif
