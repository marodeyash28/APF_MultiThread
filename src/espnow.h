#ifndef ESPNOW_H
#define ESPNOW_H

#include <esp_now.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <map> 
#include <string> 
#include "global.h"

// Global variables
extern unsigned long esp_Status_TimerCurrent;
extern unsigned long esp_Status_TimerStart;
extern unsigned long esp_Status_Timer;

extern bool esp_Device_Paired;
extern bool esp_Filter_Cover;
extern String esp_Prev_Fan_Speed;
extern String esp_Current_Fan_Speed;

extern int esp_Current_Ov_Flag;
extern int esp_Current_State;
extern int esp_Prev_Ov;

// Declare global map for peer sensors
extern std::map<std::string, std::map<std::string, std::string>> esp_Peer_Sensors;

extern StaticJsonDocument<256> esp_Purifer_Status;
extern StaticJsonDocument<256> esp_My_Status;

// Broadcast address for ESPNOW
extern unsigned char esp_Broadcast_Address[6];
extern QueueHandle_t esp_Espnow_Queue;

typedef enum {
  ESPNOW_RECV_CB,
  ESPNOW_SEND_CB,
  ESPNOW_CLZ_CB,
  ESPNOW_CLZ_DEL_PEER
} espnow_event_id_t;

typedef enum {
  UNICAST,
  MULTICAST,
  BROADCAST,
  TEMP_UNICAST
} clz_espnow_broadcast_mode;

typedef struct {
  espnow_event_id_t id;
  clz_espnow_broadcast_mode broadcast_mode;
  uint8_t mac_addr[6];
  uint8_t *data;
  int data_len;
  String sensorData;
} clz_espnow_event_t;


void startESPNOW();
void espnowFilterHandler(int filterStatus);
void publish_my_status();
void control_speed(String fanSpeed, int iState);
void espnow_recv_cb(const uint8_t *i_mac_addr, const uint8_t *data, int len);
void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status);
void espnow_task(void *pvParameter);
void send_apf_status(const uint8_t *mac_addr, clz_espnow_broadcast_mode iMode);
void formatMacAddress(const uint8_t *mac_addr, char *buffer, int maxLength);
bool isPeerPresent(const String &temp);
void removePeer(const uint8_t *peer_addr);
void calculateFanSpeed(String &_fanSpeed, int &iState);
void addPeer(const uint8_t *peer_addr);
void broadcast_on_espnow(const uint8_t *messageBuffer, size_t messageLength, const uint8_t *iMacAddress, clz_espnow_broadcast_mode iBroadcast_Mode);
bool sendToQueue(const uint8_t *mac_addr, clz_espnow_event_t *event);
int safeStringToInt(const std::__cxx11::string &str);
void convertMacAddress(String macStr, uint8_t *macArray);

#endif // ESPNOW_H
