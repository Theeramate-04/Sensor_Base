#include <Arduino.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <string>
#include <WebServer.h>
#include <ArduinoJson.h>

#include "cfg/host.h"
#include "common/msg_util.h"
#include "common/nvs_function.h"
#include "common/server_config.h"

WebServer server(80);
Config config;

volatile bool configSaved = true;

void handleGetConfig(void) {
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

void handleSetConfig(void) {
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

void HTMLhandleSaveConfig(void) {
  String AP_ssid = server.arg("ssid");
  String AP_password = server.arg("password");
  String MQTT_server = server.arg("MQTT_SERVER_H");
  String MQTT_port = server.arg("MQTT_PORT_H");
  String MQTT_token = server.arg("MQTT_TOKEN_H");

  NVS_Write("WIFI_SSID_S", AP_ssid.c_str());
  NVS_Write("WIFI_PWD_S",  AP_password.c_str());
  NVS_Write("MQTT_SERVER_S", MQTT_server.c_str());
  NVS_Write("MQTT_PORT_S", MQTT_port.toInt());
  NVS_Write("MQTT_TOKEN_S",  MQTT_token.c_str());

  server.send(200, "text/html", "Config saved! Rebooting...");

  delay(1000);
  configSaved = false;
  ESP.restart();
}

void HTMLhandleConfig(void) {
  String html = "<html><body><h1>Configure WiFi</h1>"; 
  html += "<form method='POST' action='/saveConfig'>";
  html += "<br><label for='ssid'>WIFI SSID:</label>";
  html += "<input type='text' id='ssid' name='ssid' required><br>";
  html += "<br><label for='password'>WIFI Password:</label>";
  html += "<input type='password' id='password' name='password' required><br>";
  html += "<br><label for='MQTT_SERVER_H'>MQTT Server:</label>";
  html += "<input type='text' id='MQTT_SERVER_H' name='MQTT_SERVER_H' required><br>";
  html += "<br><label for='MQTT_PORT_H'>MQTT Port:</label>";
  html += "<input type='number' id='MQTT_PORT_H' name='MQTT_PORT_H' required><br>";
  html += "<br><label for='MQTT_TOKEN_H'>MQTT Token:</label>";
  html += "<input type='text' id='MQTT_TOKEN_H' name='MQTT_TOKEN_H' required><br>";
  html += "<br><input type='submit' value='Save'><br>";
  html += "</form></body></html>";
  server.send(200, "text/html", html);
  HTMLhandleSaveConfig();
}

void setupAP(void) {
  WiFi.softAP(FSP32_SSID, ESP32_PWD);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  server.on("/", HTMLhandleConfig);
  //server.on("/saveConfig", HTTP_POST, HTMLhandleSaveConfig);
  server.on("/getConfig", HTTP_GET, handleGetConfig);
  server.on("/setConfig", HTTP_POST, handleSetConfig);
  server.begin();
  Serial.println("HTTP server started");
  while (configSaved) {delay(100);}
}
