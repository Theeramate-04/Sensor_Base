#include <Arduino.h>
#include <Adafruit_SHTC3.h>
#include "tsk/sensor_task.h"
#include "common/msg_util.h"

void tsk_sensoe_send_data(void);

extern Adafruit_SHTC3 shtc3;
extern QueueHandle_t sensorQueue;
extern QueueHandle_t mqttQueue;

static int tsk_sensor_freg_ms = 60000;
static int tsk_sensor_wait_ms = 50;

void tsk_sensor_send_data(void) {
  int rc;
  Queue_msg out_msg;

  out_msg.msg_evt = msg_evt_t::SENSOR_DATA;
  sensors_event_t humidity, temp;
  shtc3.getEvent(&humidity, &temp);
  out_msg.dsensor.Datatemp = temp.temperature;
  out_msg.dsensor.Datahumid = humidity.relative_humidity;
  
  rc = xQueueSend(mqttQueue, &out_msg, tsk_sensor_wait_ms);
  if (rc == pdTRUE) {
    Serial.println("TSK_Sensor:Report data OK");
  }
  else {
    Serial.printf("TSK_Sensor:Report data FAIL %d\r\n", rc);
  }
}

void sensor_entry(void *pvParameters) {
  int rc; 
  unsigned long tsk_ms = millis();
  Queue_msg in_msg;

  Serial.printf("TSK_Sensor:Start\r\n");

  while (1) {
    rc = xQueueReceive(sensorQueue, &in_msg, tsk_sensor_wait_ms);
    if (rc == pdPASS) {
      switch (in_msg.msg_evt) {
        case msg_evt_t::FREG_CFG : {
          tsk_sensor_freg_ms = in_msg.dmqtt.f_sensor * 1000;
          break;
        }
        default:{
          Serial.printf("TSK_Sensor:Not support event %d\r\n", in_msg.msg_evt);
          break;
        }
      }
    }
    else {
      if (millis() - tsk_ms > tsk_sensor_freg_ms ){
        tsk_ms = millis();
        tsk_sensor_send_data();
      }
    }
  }
  Serial.printf("TSK_Sensor:End\r\n");
}

