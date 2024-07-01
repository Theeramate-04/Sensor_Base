#include <Arduino.h>
#include <PubSubClient.h>
#include <Adafruit_SHTC3.h>
#include <SPI.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <WebServer.h>

#include "common/msg_util.h"
#include "common/nvs_function.h"
#include "common/server_config.h"
#include "tsk/gps_task.h"
#include "tsk/mqtt_task.h"
#include "tsk/sensor_task.h"

Adafruit_SHTC3 shtc3 = Adafruit_SHTC3();

QueueHandle_t sensorQueue;
QueueHandle_t gpsQueue;
QueueHandle_t mqttQueue;
extern WebServer server;

void setup() {
  Serial.begin(115200);
  shtc3.begin();
  msg_util_init();
  init_nvs();
  
  sensorQueue = xQueueCreate(10, sizeof(Queue_msg));
  gpsQueue = xQueueCreate(10, sizeof(Queue_msg));
  mqttQueue = xQueueCreate(10, sizeof(Queue_msg));

  xTaskCreate(mqtt_entry, "MQTT_SERVICE", 8192, NULL, 1, NULL);
  xTaskCreate(sensor_entry, "SENSOR_DATA", 2000, NULL, 1, NULL);
  xTaskCreate(gps_entry, "SENSOR_SERVICE", 2000, NULL, 1, NULL);
  Serial.println("Hello");
}


void loop() {server.handleClient();}