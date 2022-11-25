#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>

#include "bgTask.h"
#include "charger.h"
#include "discharger.h"
#include "auth.h"
#include "Sun2000softLimiterEmu.h"  //TBR

/* ESP32 DEV MODUL */

/*
   MQTT
*/
#define MQTT_SUFFIX           "/bat/"
const char *name = "Battery";
const char *mqtt_broker = "apollo.local";
const char *sdmPwr_topic = "/sdmGw/power_quick";

PubSubClient ps_client;
WiFiClient espClient;

/* power informartion */
const int32_t pwrFlowDelay = 3;
int32_t pwrFlow = 0;
float currentPwr = 0.0f;
uint32_t lastPwr_ms = 0;

uint32_t nextDataSync_ms = 5000; 

/* sun2000 info */
//note: voltages seems to be read 0.4V'ish too high by my unit (reference is JK bms)
Background::sun2k_info_t s2kInfo = {};
const float vbat_min = 60.8f;
const float vbat_dchgActivate = 62.9f;
const float vbat_emergencyChg = 60.0f;
const float vbat_boostChgMax = 69.0f;     //note: top normal charger is 69.2, which is true 68.7V (3.435V per cell)
bool isEmergencyChg = false;

/* temperature */
Background::temp_info_t tempInfo;
#define FAN_PIN                   15
#define HEATER_PIN                22
const float fanStart_c = 27.0f;
const float fanStop_c = 23.5f;
const float heaterStart_c = 6.0f;
const float heaterStop_c = 10.0f;
const float shutdown_c = 37.0f;
const float minCharge_c = 4.0f;

void WiFiEvent(WiFiEvent_t event)
{
  switch (event)
  {
    case SYSTEM_EVENT_STA_START:
      //set sta hostname here
      WiFi.setHostname(name);
      break;
    case SYSTEM_EVENT_STA_CONNECTED:
      //enable sta ipv6 here
      WiFi.enableIpV6();
      break;
    case SYSTEM_EVENT_AP_STA_GOT_IP6:
      //both interfaces get the same event
      Serial.print("STA IPv6: ");
      Serial.println(WiFi.localIPv6());
      Serial.print("AP IPv6: ");
      Serial.println(WiFi.softAPIPv6());
      break;
    case SYSTEM_EVENT_STA_GOT_IP:
      //Serial.println("STA Connected");
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      Serial.println("STA Disconnected");
      break;
    default:
      break;
  }
}

bool reconnect()
{
  uint32_t i = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");

    /* 30sec timeout */
    if (++i > 60)
      return false;
  }

  if (ps_client.connect(name) == false)
  {
    Serial.print("rc=");
    Serial.print(ps_client.state());
    Serial.print(" ");

    return false;
  }

  /* subscribe to all */
  Serial.print("[subscr: ");
  Serial.print(sdmPwr_topic);
  Serial.print("] ");
  ps_client.subscribe(sdmPwr_topic);

  /* debug */
  char msg[128];
  snprintf(msg, sizeof(msg), "%s connected from %s (%s)", name, WiFi.localIP().toString().c_str(), WiFi.localIPv6().toString().c_str());
  ps_client.publish("/debug", msg);

  return true;
}

void ps_callback(char* topic, byte* payload, unsigned int length)
{
//  Serial.print("Rx Msg [");
//  Serial.print(topic);
//  Serial.print("] ");
//  for (int i = 0; i < length; i++)
//    Serial.print((char)payload[i]);
//  Serial.println();

  DynamicJsonDocument doc(256);
  DeserializationError error = deserializeJson(doc, payload);
  if (error)
  {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }

  /* determine forward power */
  currentPwr = (doc.containsKey("L1") and doc.containsKey("L2") and doc.containsKey("L3")) ? (((float)doc["L1"]) + ((float)doc["L2"]) + ((float)doc["L3"])) : 0.0f;
  lastPwr_ms = millis();
#if 0
  Serial.print("pwrFlow:");
  Serial.print(pwrFlow);
  Serial.print(" chg:");
  Serial.print(charger.active);
  Serial.print(" disChg:");
  Serial.println(discharger.active);
#endif

  /* temperature check */
  if(lastPwr_ms > tempInfo.tstamp+10000 or tempInfo.temp == DEVICE_DISCONNECTED_C)
  {
    Serial.println("No tempInfo\n");
    discharger.off();
    charger.off();
    pwrFlow = 0;
    return;
  }
  if(tempInfo.temp > shutdown_c)
  {
    Serial.println("HOT!!\n");
    discharger.off();
    charger.off();
    pwrFlow = 0;
    return;
  }

  /* prevent inverter powering charger and vise versa */
  if(charger.active and discharger.active)
  {
    Serial.println("WOW!!!! Discharger and Charger both active!!!");
    if(currentPwr > 0.0f)
      charger.off();
    else
      discharger.off();
    pwrFlow = 0;
    return;
  }

  /* start up decice */
  if(charger.active == false and discharger.active == false)
  {
    if(currentPwr > 0.0f and pwrFlow < pwrFlowDelay)
      pwrFlow++;
    else if(currentPwr < 0.0f and pwrFlow > -pwrFlowDelay)
      pwrFlow--;
  }

  /* prevent inverter powering charger and vise versa */
  //note: in case of active device, deactivate first and try ned round
  if(charger.active or pwrFlow <= -pwrFlowDelay)
  {
    /* no charging below minCharge_c */
    if(tempInfo.temp < minCharge_c)
    {
      Serial.print("Too cold\n");
      charger.off();
      pwrFlow = 0;
    }
    else
    {      
      charger.setMaxPower_w(currentPwr, s2kInfo.vbat_v <= vbat_boostChgMax and s2kInfo.tstamp+10000 > lastPwr_ms);
      if(charger.active == false)
        pwrFlow = 0;
    }
  }
  else if(discharger.active or pwrFlow >= pwrFlowDelay)
  {
    /* ensure not to under voltage the battery */
    if(s2kInfo.vbat_v < vbat_min or s2kInfo.tstamp+10000 < lastPwr_ms)
    {
      Serial.print("Battery empty\n");
      discharger.off();
      pwrFlow = 0;
    }    
    else if(discharger.active == false and s2kInfo.vbat_v < vbat_dchgActivate)
    {
      Serial.print("Battery too low\n");
      pwrFlow = 0;
    }
    else
    {
      discharger.setMaxPower_w(currentPwr);
      if(discharger.active == false)
        pwrFlow = 0;
    }
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.println("--== Battery ==--");

  /* FAN */
  //note: unknown state during boot -> start cooling
  pinMode(FAN_PIN, OUTPUT);
  digitalWrite(FAN_PIN, HIGH);

  /* HEATER */
  pinMode(HEATER_PIN, OUTPUT);
  digitalWrite(HEATER_PIN, LOW);

  /* init external hardware */
  discharger.setup();
  charger.setup();

  WiFi.disconnect(true);
  WiFi.onEvent(WiFiEvent);
  WiFi.begin(WIFI_SSID, WIFI_PW);

  Serial.print("Wifi ");
  uint32_t i=0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");

    /* 30sec timeout */
    if (++i > 60)
      ESP.restart();
  }

  Serial.print(" connected. IP address: ");
  Serial.println(WiFi.localIP());

  WiFi.setHostname(name);

  ps_client.setClient(espClient);
  ps_client.setServer(mqtt_broker, 1883);
  ps_client.setCallback(ps_callback);

  /* sun 2000 rs232 + temp sensor */
  bg.start();

  /* OTA */
  ArduinoOTA
  .onStart([]()
  {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  })
  .onEnd([]() {
    Serial.println("\nEnd");
  })
  .onProgress([](unsigned int progress, unsigned int total) {
    /*Serial.printf("Progress: %u%%\r", (progress / (total / 100)));*/
  })
  .onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.setHostname(name);
  ArduinoOTA.setPassword(WIFI_PW);
  ArduinoOTA.begin();
}

void loop()
{
  /* MQTT */
  if (ps_client.connected() == false)
  {
    Serial.print("Connecting to MQTT ");
    if (reconnect() == false)
    {
      Serial.println("failed.");
      delay(5000);
      ESP.restart();             //supposed to not return...
      return;
    }
    Serial.println("success.");
  }

  ps_client.loop();

  /* foward statistics */
  if(nextDataSync_ms < millis())
  {

    /* additional sun2000 info */
    //note: uncritical
    Background::sun2k_info_t nextInfo = bg.getS2kInfo();
    if(s2kInfo.tstamp != nextInfo.tstamp)
    {
      s2kInfo = nextInfo;
      Serial.print("S2kIno: bat=");
      Serial.print(s2kInfo.vbat_v);
      Serial.print("V, avgPwr=");
      Serial.print(s2kInfo.avgPwr_w);
      Serial.print("W, enToday=");
      Serial.print(s2kInfo.energyToday_kwh);
      Serial.println("kWh");
      
      ps_client.publish( MQTT_SUFFIX "bat_v", String(s2kInfo.vbat_v, 1).c_str());
      ps_client.publish( MQTT_SUFFIX "today_kwh", String(s2kInfo.energyToday_kwh, 1).c_str());
      ps_client.publish( MQTT_SUFFIX "avgPwr_w", String(s2kInfo.avgPwr_w, 1).c_str());

      /* emergency charging */
      if(isEmergencyChg == false and s2kInfo.vbat_v > vbat_emergencyChg/2.0f and s2kInfo.vbat_v <= vbat_emergencyChg and charger.active == false)
      {
        isEmergencyChg = true;
        charger.emergencyCharge(true);
      }
      else if(isEmergencyChg == true and (s2kInfo.vbat_v > vbat_min or discharger.active))
      {
        isEmergencyChg = false;
        charger.emergencyCharge(false);
      }
    }    
    else
    {
      Serial.println("WRN: sun2k info no tstamp update");
      isEmergencyChg = false;
    }

    /* share max allowed charging power */
    ps_client.publish( MQTT_SUFFIX "chargerMax_w", String(-charger.getMaxPower_w(), 0).c_str());

    /* reply state */
    char json[256];
    snprintf(json, sizeof(json), "{\"pwr\": %.f, \"chg\": %.f, \"dchg\": %.f, \"dchgAvg\": %.f, \"state\": %d, \"up\": %u, \"fChgInd\": %u, \"isEmergencyChg\": %u, \"boostChg\": %u}", 
      currentPwr, charger.getMaxPower_w(), discharger.getMaxPower_w(), discharger.active ? s2kInfo.avgPwr_w : 0.0f, pwrFlow, millis(), !digitalRead(charger.nFullyCharged), isEmergencyChg, 
      digitalRead(charger.boostPin));
    ps_client.publish( MQTT_SUFFIX "stat", json);

    /* temperature information */
    //note: critical
    Background::temp_info_t nextTemp = bg.getTempInfo();
    if(tempInfo.tstamp != nextTemp.tstamp)
    {
      tempInfo = nextTemp;
      Serial.print("Temp: ");
      Serial.print(tempInfo.temp);
      Serial.println("C");    
      ps_client.publish( MQTT_SUFFIX "temp_c", String(tempInfo.temp, 1).c_str());
      ps_client.publish( MQTT_SUFFIX "fan", digitalRead(FAN_PIN) ? "on" : "off");
    }

    /* temperture control */
    if(tempInfo.temp > fanStart_c or tempInfo.temp == DEVICE_DISCONNECTED_C)
      digitalWrite(FAN_PIN, HIGH);
    else if(tempInfo.temp < fanStop_c)
      digitalWrite(FAN_PIN, LOW);
    if(tempInfo.temp < heaterStart_c and tempInfo.temp != DEVICE_DISCONNECTED_C and ((lastPwr_ms < millis()+10000 and currentPwr < -60.0f) or isEmergencyChg))
      digitalWrite(HEATER_PIN, HIGH);
    else if(tempInfo.temp > heaterStop_c or tempInfo.temp == DEVICE_DISCONNECTED_C or (currentPwr > 0.0f and isEmergencyChg == false) or lastPwr_ms > millis()+10000)
      digitalWrite(HEATER_PIN, LOW);

    nextDataSync_ms = millis() + 7500;
  }

  /* ensure regular updates */
  if(millis() > lastPwr_ms+5000)
  {
    Serial.println("No SDM data!");
    discharger.off();
    charger.off();
    pwrFlow = 0;

    char json[128];
    snprintf(json, sizeof(json), "{\"wrnNoSdm\": %ld}", (int64_t)(millis() - lastPwr_ms));
    ps_client.publish( MQTT_SUFFIX "stat", json);

    delay(500);
  }

  ArduinoOTA.handle();
  delay(10);
}
