#ifndef MSG_UTIL_H
#define MSG_UTIL_H

enum msg_evt_t {
  GPS_DATA,
  SENSOR_DATA,
  FREG_CFG
};

struct sensor_data {
    float Datatemp;
    float Datahumid;
};

struct gps_data {
    float latitude;
    float longitude;
};

struct mqtt_data {
    int f_sensor;
    int f_gps;
};

struct Queue_msg {
  msg_evt_t msg_evt;
  union {
    struct sensor_data dsensor;
    struct gps_data dgps;
    struct mqtt_data dmqtt;
  } ;
};

enum queue_id_t {
  GPS_QUEUE,
  SENSOR_QUEUE,
  MQTT_QUEUE,
  QUEUE_ID_END
};

enum TopicType {
    RPC_RESPONSE,
    ATTRIBUTES_RESPONSE,
    ATTRIBUTES,
    FW_RESPONSE,
    UNKNOWN
};

struct Config {
  char WIFI_SSID_Config[100];
  char WIFI_PWD_Config[100];
  char MQTT_SERVER_Config[100];
  int MQTT_PORT_Config ;
  char MQTT_ACCESS_TOKEN_Config[100];
};

int msg_util_init(void);
int msg_queue_send(queue_id_t queue_id, Queue_msg* msg, int timeout);
int msg_queue_recv(queue_id_t queue_id, Queue_msg* msg, int timeout);

#endif 