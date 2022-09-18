#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>

#include "sun2000softLimiterEmu.h"
#include "auth.h"

/* ESP32 WROOM */

/*
   MQTT
*/

const char *name = "Battery";
const char *mqtt_broker = "apollo.local";
const char *sdmPwr_topic = "/sdmGw/power_quick";

PubSubClient ps_client;
WiFiClient espClient;

float lastFwdPwr_w = 0.0f;
const float fwdPwrLowPath_factor = 0.5f;    //limits: 1.0 -> immedieatly full power forward. 0.0 -> no power forward

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
      Serial.println("STA Connected");
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
  //note: smooth true power consuption slightly for importing power in order not to overshoot here
  float currentPwr = (doc.containsKey("L1") and doc.containsKey("L2") and doc.containsKey("L3")) ? (((float)doc["L1"]) + ((float)doc["L2"]) + ((float)doc["L3"])) : 0.0f;
  if(lastFwdPwr_w < currentPwr and currentPwr > 0.0f)
    lastFwdPwr_w = (1.0f-fwdPwrLowPath_factor)*lastFwdPwr_w + fwdPwrLowPath_factor*currentPwr;
  else
    lastFwdPwr_w = currentPwr;
  softLimiterEmu.set(lastFwdPwr_w);

  //todo switch between charging and discahrging

  //tbr
  Serial.print(currentPwr);
  Serial.print("->");
  Serial.println(lastFwdPwr_w);
}

void setup()
{
  Serial.begin(115200);
  
  softLimiterEmu.setup();

  WiFi.disconnect(true);
  WiFi.onEvent(WiFiEvent);
  WiFi.begin(WIFI_SSID, WIFI_PW);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println(" connected.");
  Serial.println("IP address:");
  Serial.println(WiFi.localIP());

  WiFi.setHostname(name);

  ps_client.setClient(espClient);
  ps_client.setServer(mqtt_broker, 1883);
  ps_client.setCallback(ps_callback);

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
    Serial.print(millis());
    Serial.print(": Connecting to MQTT ");
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

}
