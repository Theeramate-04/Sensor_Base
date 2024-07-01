#include <Arduino.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <string>
#include <WebServer.h>
#include <ArduinoJson.h>

#include "common/msg_util.h"
#include "common/nvs_function.h"
#include "common/server_config.h"

WebServer server(80);
Config config;

void handleGetConfig() {
  JsonDocument doc;
  doc["WIFI_SSID"] = config.WIFI_SSID_Config;
  doc["WIFI_PWD"] = config.WIFI_PWD_Config;
  doc["MQTT_SERVER"] = config.MQTT_SERVER_Config;
  doc["MQTT_PORT"] = config.MQTT_PORT_Config;
  doc["MQTT_ACCESS_TOKEN"] = config.MQTT_ACCESS_TOKEN_Config;

  String response;
  serializeJson(doc, response);

  server.send(200, "application/json", response);
}

void handleSetConfig() {
    if (server.hasArg("plain")) {
    String body = server.arg("plain");
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
      server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }

    if (doc.containsKey("WIFI_SSID_Config") && doc.containsKey("WIFI_PWD_Config") &&
        doc.containsKey("MQTT_SERVER_Config") && doc.containsKey("MQTT_PORT_Config") &&
        doc.containsKey("MQTT_ACCESS_TOKEN_Config")) {
      
      strcpy(config.WIFI_SSID_Config, doc["WIFI_SSID_Config"]);
      strcpy(config.WIFI_PWD_Config, doc["WIFI_PWD_Config"]);
      strcpy(config.MQTT_SERVER_Config, doc["MQTT_SERVER_Config"]);
      config.MQTT_PORT_Config = doc["MQTT_PORT_Config"];
      strcpy(config.MQTT_ACCESS_TOKEN_Config, doc["MQTT_ACCESS_TOKEN_Config"]);

      NVS_Write("WIFI_SSID_S", config.WIFI_SSID_Config);
      NVS_Write("WIFI_PWD_S", config.WIFI_PWD_Config);
      NVS_Write("MQTT_SERVER_S", config.MQTT_SERVER_Config);
      NVS_Write("MQTT_PORT_S", config.MQTT_PORT_Config);
      NVS_Write("MQTT_TOKEN_S", config.MQTT_ACCESS_TOKEN_Config);
          
      server.send(200, "application/json", "{\"status\":\"Config updated\"}");
    } else {
      server.send(400, "application/json", "{\"error\":\"Missing parameters\"}");
    }
  } else {
    server.send(400, "application/json", "{\"error\":\"Missing JSON body\"}");
  }
}