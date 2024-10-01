#ifndef HOTSPOT_H
#define HOTSPOT_H

#include <string>
#include <Arduino.h>

#include "global.h"

extern bool htp_Filter_Cover;
extern bool htp_Wifi_Pin_Status;
extern bool htp_Wifi_Connected;
extern bool htp_LED_State;
extern bool htp_Wifi_K_Status;
extern bool htp_Hold;
extern int htp_UpdFirm;
extern int htp_Scanned_Devices;
extern unsigned long long htp_Filter_TimerCurrent;
extern bool htp_Flickering_Done;

extern QueueHandle_t commandQueue;


void startHotspot();
void hotspotFilterHandler(int filterStatus);
void handleRoot();
void handleCommand();
void handleStatus();
void handleSaveWiFi();
void control_h_speed(String fanSpeed, int iState);
void commandTask(void *pvParameters);
void clientTask(void *pvParameters);



#endif // HOTSPOT_H
