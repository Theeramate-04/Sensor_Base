#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <string>
#include <WebServer.h>

#include "cfg/host.h"
#include "cfg/config.h"
#include "tsk/mqtt_task.h"
#include "common/msg_util.h"
#include "common/nvs_function.h"
#include "common/server_config.h"

static void tsk_mqtt_snd_gps(Queue_msg in_msg);
static void wifi_config(void);
static void mqtt_config(void);
static void mqtt_reconnect(void);
static void mqtt_callback(char *topic, byte *payload, unsigned int length);
static void nwk_check(void);
static void requestFirmwareChunk(void);
static TopicType getTopicType(const String& topic);

extern QueueHandle_t sensorQueue;
extern QueueHandle_t gpsQueue;
extern QueueHandle_t mqttQueue;
extern Config config;
extern WebServer server;

static int tsk_mqtt_wait_ms = 100;
static int requestId = 0;
static int chunkIndex = 0;
static bool updating = false;
static size_t totalLength = 0;
static size_t currentLength = 0;
static float fw_version_old = 0;
static float fw_version_new = 0;
const char* fw_title_old = nullptr;
const char* fw_title_new = nullptr;


WiFiClient espClient;
PubSubClient client(espClient);

void wifi_config(void) {

  NVS_Read("WIFI_SSID_S", WIFI_SSID_S);
  NVS_Read("WIFI_PWD_S", WIFI_PWD_S);
  strcpy(config.WIFI_SSID_Config, WIFI_SSID_S);
  strcpy(config.WIFI_PWD_Config, WIFI_PWD_S);

  WiFi.begin(WIFI_SSID_S, WIFI_PWD_S);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("TSK_MQTT:WiFi connected");
  Serial.print("TSK_MQTT:IP address: ");
  Serial.println(WiFi.localIP());
}

void mqtt_config(void) {
  NVS_Read("MQTT_SERVER_S", MQTT_SERVER_S);
  NVS_Read("MQTT_PORT_S", &MQTT_PORT_S);
  strcpy(config.MQTT_SERVER_Config, MQTT_SERVER_S);
  config.MQTT_PORT_Config = MQTT_PORT_S;

  client.setServer(MQTT_SERVER_S, MQTT_PORT_S);
  client.setCallback(mqtt_callback);
  client.setBufferSize(2048);

  while (!client.connected()) {
      mqtt_reconnect();
  }
}

void mqtt_reconnect(void) {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = WiFi.macAddress();
    
    NVS_Read("MQTT_TOKEN_S", MQTT_TOKEN_S);
    strcpy(config.MQTT_ACCESS_TOKEN_Config, MQTT_TOKEN_S);

    if (client.connect(clientId.c_str(), MQTT_TOKEN_S, NULL)) {
      Serial.println("connected");
      client.subscribe("v1/devices/me/rpc/response/+");
      client.publish("v1/devices/me/rpc/request/1", "{\"method\":\"getCurrentTime\",\"params\":{}}",1);
      
      client.subscribe("v1/devices/me/attributes/response/+");
      client.publish("v1/devices/me/attributes/request/1", "{\"sharedKeys\":\"fw_title,fw_version,fw_size\"}");
      
      client.subscribe("v1/devices/me/attributes");

    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      delay(5000);
    }
  }
}

void requestFirmwareChunk(void) {
  char topic[128];
  snprintf(topic, sizeof(topic), "v2/fw/request/%d/chunk/%d", requestId, chunkIndex);
  client.publish(topic, String(firmware_chunk_size).c_str(), 1);
}

void tsk_mqtt_snd_gps(Queue_msg in_msg) {
  Serial.print("latitude: ");
  Serial.println(in_msg.dgps.latitude);
  Serial.print("longitude: ");
  Serial.println(in_msg.dgps.longitude);
  
  char laStr[16];
  dtostrf(in_msg.dgps.latitude, 1, 2, laStr);
  char longStr[16];
  dtostrf(in_msg.dgps.longitude, 1, 2, longStr);
  
  JsonDocument doc;
  doc["gps"] = "gps";
  JsonObject dataGps = doc["dataGps"].to<JsonObject>();
  dataGps["latitude"] = laStr;
  dataGps["longitude"] = longStr;
  char jsonBuffer[256];
  serializeJson(doc, jsonBuffer);
  client.publish("v1/devices/me/telemetry", jsonBuffer);
}

void tsk_mqtt_snd_sensor(Queue_msg in_msg) {
  Serial.print("Temperature: ");
  Serial.print(in_msg.dsensor.Datatemp);
  Serial.println(" degrees C");
  Serial.print("Humidity: ");
  Serial.print(in_msg.dsensor.Datahumid);
  Serial.println("% rH");

  char tempStr[16];
  dtostrf(in_msg.dsensor.Datatemp, 1, 2, tempStr);
  char humidStr[16];
  dtostrf(in_msg.dsensor.Datahumid, 1, 2, humidStr);

  JsonDocument doc;
  doc["sensor"] = "sensor";
  JsonObject dataSensor = doc["dataSensor"].to<JsonObject>();
  dataSensor["Temperature"] = tempStr;
  dataSensor["Humid"] = humidStr;
  char jsonBuffer[256];
  serializeJson(doc, jsonBuffer);
  client.publish("v1/devices/me/telemetry", jsonBuffer);
}

void tsk_freg_send_gps(int freg) {
  int rc;
  Queue_msg out_msg;

  out_msg.msg_evt = msg_evt_t::FREG_CFG;
  out_msg.dmqtt.f_gps = freg;

  rc = xQueueSend(gpsQueue, &out_msg, tsk_mqtt_wait_ms);  
  if (rc == pdTRUE) {
    Serial.println("FREG_CFG_GPS:Report data OK");
  }
  else {
    Serial.printf("FREG_CFG_GPS:Report data FAIL %d\r\n", rc);
  }
}

void tsk_freg_send_sensor(int freg) {
   int rc;
  Queue_msg out_msg;

  out_msg.msg_evt = msg_evt_t::FREG_CFG;
  out_msg.dmqtt.f_sensor = freg;

  rc = xQueueSend(sensorQueue, &out_msg, tsk_mqtt_wait_ms);  
  if (rc == pdTRUE) {
    Serial.println("FREG_CFG_Sensor:Report data OK");
  }
  else {
    Serial.printf("FREG_CFG_Sensor:Report data FAIL %d\r\n", rc);
  }
}

void nwk_check(void) {

  if (client.connected() == true) {
    client.loop();
  }
  else {
    
    mqtt_reconnect();
  }
}

TopicType getTopicType(const String& topic) {
    if (topic.startsWith("v1/devices/me/rpc/response/")) return RPC_RESPONSE;
    if (topic.startsWith("v1/devices/me/attributes/response/")) return ATTRIBUTES_RESPONSE;
    if (topic.startsWith("v1/devices/me/attributes")) return ATTRIBUTES;
    if (topic.startsWith("v2/fw/response/")) return FW_RESPONSE;
    return UNKNOWN;
}

void mqtt_callback(char *topic, byte *payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] Payload length: ");
  Serial.println(length);
  Serial.println(String((char*)payload).substring(0, length));
  Serial.println("-----------------------");
  
  String topicStr = String(topic);
  TopicType topicType = getTopicType(topicStr);
  
  switch (topicType) {
    case RPC_RESPONSE: {
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload, length);
      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return;
      }
      int frequencySenosr = atoi(doc["frequency_Senosr"]);
      int frequencyGPS = atoi(doc["frequency_GPS"]);
      tsk_freg_send_sensor(frequencySenosr);
      tsk_freg_send_gps(frequencyGPS);
      break;
    }
  
    case ATTRIBUTES_RESPONSE: {
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload, length);
      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return;
      }
      fw_title_old = strdup(doc["shared"]["fw_title"].as<const char*>());
      fw_version_old = atof(doc["shared"]["fw_version"]);
      totalLength = doc["shared"]["fw_size"];
      break;
    }

    case ATTRIBUTES: {
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload, length);
      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return;
      }
      if (doc.containsKey("deleted")) {return;}
      fw_title_new = strdup(doc["fw_title"].as<const char*>());
      fw_version_new = atof(doc["fw_version"]);
      totalLength = doc["fw_size"];
      if (strcmp(fw_title_old, fw_title_new) == 0 && fw_version_new > fw_version_old) {
        client.subscribe("v2/fw/response/+/chunk/+");
        requestId++;
        chunkIndex = 0;  
        currentLength = 0;
        updating = true;
        if (!Update.begin(totalLength)) {
          Serial.println("Update begin failed");
          return;
        }
        requestFirmwareChunk();
      }
      break;
    } 

    case FW_RESPONSE: {
      size_t written = Update.write(payload, length);
      currentLength += written;
      if (written != length) {
        Serial.println("Failed to write chunk");
        Update.abort();
        updating = false;
      } 
      else {
        Serial.printf("Written chunk %d, total %d/%d\n", chunkIndex, currentLength, totalLength);
        chunkIndex++;
          
        if (currentLength == totalLength) {
          Serial.println("Update complete");
          if (Update.end(true)) {
            Serial.println("OTA Update successful, restarting...");
            ESP.restart();
          } 
          else {
            Serial.println("Update failed");
            Update.abort();
            updating = false;
          }
        } 
        else {
          requestFirmwareChunk();
        }
      }
      break;
    }
    
    case UNKNOWN:
        default:
            Serial.println("Unknown topic");
            break;
  }
}

void mqtt_entry(void *pvParameters){
  int rc;
  Queue_msg in_msg;
  
  if(NVS_Read("WIFI_SSID_S", WIFI_SSID_S) == ESP_ERR_NVS_NOT_FOUND){
    NVS_Write("WIFI_SSID_S", WIFI_SSID);
  }
  if(NVS_Read("WIFI_PWD_S", WIFI_PWD_S) == ESP_ERR_NVS_NOT_FOUND){
    NVS_Write("WIFI_PWD_S", WIFI_PWD);
  }
  if(NVS_Read("MQTT_SERVER_S", MQTT_SERVER_S) == ESP_ERR_NVS_NOT_FOUND){
    NVS_Write("MQTT_SERVER_S", MQTT_SERVER);
  }
  if(NVS_Read("MQTT_PORT_S", &MQTT_PORT_S) == ESP_ERR_NVS_NOT_FOUND){
    NVS_Write("MQTT_PORT_S", MQTT_PORT);
  }
  if(NVS_Read("MQTT_TOKEN_S", MQTT_TOKEN_S) == ESP_ERR_NVS_NOT_FOUND){
    NVS_Write("MQTT_TOKEN_S", MQTT_ACCESS_TOKEN);
  }
  
  Serial.printf("TSK_MQTT:Start\r\n");
  Serial.printf("TSK_MQTT:Config Wifi\r\n");
  wifi_config();
  Serial.printf("TSK_MQTT:Config MQTT\r\n");
  mqtt_config();

  server.on("/getConfig", HTTP_GET, handleGetConfig);
  server.on("/setConfig", HTTP_POST, handleSetConfig);
  server.begin();

  while(1) {
    // rc = msg_queue_recv(queue_id_t::MQTT_QUEUE, &in_msg, tsk_mqtt_wait_ms);
    rc = xQueueReceive(mqttQueue, &in_msg, tsk_mqtt_wait_ms);
    if (rc == pdPASS) {
      switch (in_msg.msg_evt) {
        case msg_evt_t::GPS_DATA : {
          tsk_mqtt_snd_gps(in_msg);
          break;
        }
        case msg_evt_t::SENSOR_DATA : {
          tsk_mqtt_snd_sensor(in_msg);
          break;
        }
        default : {
          Serial.printf("TSK_MQTT:Not support event %d\r\n", in_msg.msg_evt);
          break;
        }
      }
    }
    else {
      nwk_check();
    }
  }
  Serial.printf("TSK_MQTT:End\r\n");
}