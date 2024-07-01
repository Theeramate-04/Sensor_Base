#include <Arduino.h>
#include "tsk/gps_task.h"
#include "common/msg_util.h"

void tsk_gps_send_data(void);

extern QueueHandle_t gpsQueue;
extern QueueHandle_t mqttQueue;

static int tsk_gps_freg_ms = 60000;
static int tsk_gps_wait_ms = 50;

void gps_entry(void *pvParameters) {
  int rc; // return code
  unsigned long tsk_ms = millis();
  Queue_msg in_msg;

  Serial.printf("TSK_GPS:Start\r\n");

  while (1) {
    // rc = msg_queue_recv(queue_id_t::GPS_QUEUE, &in_msg, tsk_gps_wait_ms);
    rc = xQueueReceive(gpsQueue, &in_msg, tsk_gps_wait_ms);
    if (rc == pdPASS) {
      switch (in_msg.msg_evt) {
        case msg_evt_t::FREG_CFG : {
          tsk_gps_freg_ms = in_msg.dmqtt.f_gps * 1000;
          break;
        }
        default:{
          Serial.printf("TSK_GPS:Not support event %d\r\n", in_msg.msg_evt);
          break;
        }
      }
    }
    else {
      if (millis() - tsk_ms > tsk_gps_freg_ms ){
        tsk_ms = millis();
        tsk_gps_send_data();
      }
    }
  }
  Serial.printf("TSK_GPS:End\r\n");
}

void tsk_gps_send_data(void) {
  int rc;
  Queue_msg out_msg;

  out_msg.msg_evt = msg_evt_t::GPS_DATA;
  out_msg.dgps.latitude = random(-90, 90);
  out_msg.dgps.longitude = random(-90, 90);

  // rc = msg_queue_send(queue_id_t::MQTT_QUEUE, &out_msg, tsk_gps_wait_ms);
  rc = xQueueSend(mqttQueue, &out_msg, tsk_gps_wait_ms);
  if (rc == pdTRUE) {
    Serial.println("TSK_GPS:Report data OK");
  }
  else {
    Serial.printf("TSK_GPS:Report data FAIL %d\r\n", rc);
  }
}