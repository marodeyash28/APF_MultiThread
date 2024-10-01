#ifndef GLOBAL_H
#define GLOBAL_H

#include <Arduino.h>
#include <EEPROM.h>
#include <esp_now.h>
#include <ArduinoJson.h>
#include <map>
#include <WiFi.h>
#include <string>
#include <MD5Builder.h>
#include <esp_ota_ops.h>
#include <WebServer.h>
#include <Update.h>
#include <DNSServer.h>

#include <rom/rtc.h>
#include <WiFiClient.h>
#include <ESPmDNS.h>
#include <WiFiManager.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <string.h>
#include <mbedtls/md5.h>

// Pin Definitions
extern const int LED_PIN;
extern const int WIFI_CONFIG_PIN;
extern const int FILTER_COVER_PIN;

extern const int RTC_BROWNOUT_DET_LVL_2;
extern const int RESET_GPIO_PIN;
extern const int QUEUE_SIZE;
extern const int BUFFER_SIZE;

// Pinout for AC
extern const int HIGH_LED;
extern const int MED_LED;
extern const int LOW_LED;

// Pinout for Plug
extern const int PLG_BLUE_LED;
extern const int PLG_RED_LED;
extern const int PLG_LED;

// Pinout for DC
extern const int g_pwm_Pin;
extern const int g_fg_Pin;
extern const int g_fan_Power;
extern const int g_frequency;
extern const int g_pwm_Channel;
extern const int g_resolution;

// Other global variables
extern String g_md5_str;
extern String g_IP;

// Global Declarations
extern String ssid;
extern char g_chipId_String[17];

// Other Variables
extern bool espNowMode;
extern bool toggleLED;
extern bool htp_Wifi_Connected;
extern unsigned long long toggleStartTime;
extern unsigned long long  buttonPressTime; 
extern unsigned long long  hotspotStartTime; 
extern unsigned long long  modeChangeInterval; 
extern const int BUTTON_PRESS_MIN_TIMER;
extern const int BUTTON_PRESS_MAX_TIMER;

typedef enum {
  CLZ_PLG_AC,
  CLZ_APF_DC,
  CLZ_APF_AC
} clz_device_type_id;

extern clz_device_type_id g_device_Type;
extern String g_subType;

#endif // GLOBAL_H
