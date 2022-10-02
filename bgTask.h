#ifndef __BG_TASK_H
#define __BG_TASK_H

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

private:
  TaskHandle_t sun2000_task;
  portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
  sun2k_info_t s2kInfo;

  friend void dataHandler(GfSun2000Data data);
public:

    void start(void);
    sun2k_info_t getS2kInfo(void);
};

extern Background bg;

#endif
