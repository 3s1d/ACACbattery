#include <Arduino.h>

#include "bgTask.h"

Background bg;

void errorHandler(int errorId, char* errorMessage) 
{
  Serial.printf("Error response: %02X - %s\n", errorId, errorMessage);
}

extern "C" void dataHandler(GfSun2000Data data) 
{
#if 0  
  Serial.println("\n\n");
  Serial.printf("Device ID     : %s\n", data.deviceID);
  Serial.printf("AC Voltage    : %.1f\tV\n", data.ACVoltage);
  Serial.printf("DC Voltage    : %.1f\tV\n", data.DCVoltage);  
  Serial.printf("Output Power  : %.1f\tW (5min avg)\n", data.averagePower);
  Serial.printf("Custom Energy : %.1f\tkW/h (can be reseted)\n", data.customEnergyCounter);
  Serial.printf("Total Energy  : %.1f\tkW/h\n", data.totalEnergyCounter);
  Serial.println("-----------------------------");
  
  std::map<int16_t, int16_t>::iterator itr;
  for (itr = data.modbusRegistry.begin(); itr != data.modbusRegistry.end(); ++itr) {
        Serial.printf("Registry %d: %d \n", itr->first, itr->second);
  }  
#endif

  portENTER_CRITICAL(&bg.mux);
  bg.s2kInfo.vbat_v = data.DCVoltage;
  bg.s2kInfo.avgPwr_w = data.averagePower;
  bg.s2kInfo.energyToday_kwh = data.customEnergyCounter;
  bg.s2kInfo.tstamp = millis();  
  portEXIT_CRITICAL(&bg.mux);
}


void bg_task(void *arg)
{
  /* init */
  GfSun2000 GF = GfSun2000(); 
  GF.setup(Serial2);  
  GF.setDataHandler(dataHandler);
  GF.setErrorHandler(errorHandler);

  /* loop */
  while(1)
  {
    GF.readData();
    //sleep(3);
    delay(1000);
  }

  vTaskDelete(nullptr);
}

void Background::start(void) 
{
  xTaskCreatePinnedToCore(
               bg_task,           /* Task function. */
               "backgroundTask",  /* name of task. */
               10000,             /* Stack size of task */
               NULL,              /* parameter of the task */
               1,                 /* priority of the task */
               &sun2000_task,     /* Task handle to keep track of created task */
               1);                /* pin task to core 0 */
}

Background::sun2k_info_t Background::getS2kInfo(void)
{
  portENTER_CRITICAL(&mux);
  sun2k_info_t ret = s2kInfo;
  portEXIT_CRITICAL(&mux);

  return ret;
}
