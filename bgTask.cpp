#include "OneWire.h"
#include <Arduino.h>

#include "bgTask.h"

Background bg;

void errorHandler(int errorId, char* errorMessage) 
{
  Serial.printf("Sun2k err: %02X - %s\n", errorId, errorMessage);
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
  bg.task();

  vTaskDelete(nullptr);
}

void Background::task(void)
{
  /* init */
  GF = GfSun2000(); 
  GF.setup(Serial2);  
  GF.setDataHandler(dataHandler);
  GF.setErrorHandler(errorHandler);

  oneWire = OneWire(oneWirePin);
  tempsens = DallasTemperature(&oneWire);
  tempsens.begin();

  /* loop */
  while(1)
  {
    /* Send the command to get temperatures */
    tempsens.requestTemperatures();

    /* get sun2000 data */
    GF.readData();

    /* get temperature */
    //note: erros might happen as we are bitbanging it
    float tempC = tempsens.getTempCByIndex(0);
    portENTER_CRITICAL(&bg.mux);
    if(tempC == DEVICE_DISCONNECTED_C)
    {
      /* yield for 3 in a row before reporting a negative result */
      if(++tempErrorDelay > 3)
        tempInfo.temp = tempC;
    }
    else
    {
      /* everything correct */
      tempInfo.temp = tempC;
      tempErrorDelay = 0;
    }
    tempInfo.tstamp = millis();
    portEXIT_CRITICAL(&bg.mux);

    if(tempInfo.temp == DEVICE_DISCONNECTED_C) 
      Serial.println("Temp sens error.");

    delay(2000);
  }
  
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

Background::temp_info_t Background::getTempInfo(void)
{
  portENTER_CRITICAL(&mux);
  temp_info_t ret = tempInfo;
  portEXIT_CRITICAL(&mux);

  return ret;
}
