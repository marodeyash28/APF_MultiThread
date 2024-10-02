#include "global.h"
#include "espnow.h"
#include "hotspot.h"

#define EEPROM_SIZE 1

// Pin Definitions
const int LED_PIN = 21;
const int WIFI_CONFIG_PIN = 22;
const int FILTER_COVER_PIN = 23;

const int RTC_BROWNOUT_DET_LVL_2 = 2;
const int RESET_GPIO_PIN = 4;
const int QUEUE_SIZE = 50;
const int BUFFER_SIZE = 1024;

// Pinout for AC
const int HIGH_LED = 27;
const int MED_LED = 26;
const int LOW_LED = 25;

// Pinout for Plug
const int PLG_BLUE_LED = 25;
const int PLG_RED_LED = 26;
const int PLG_LED = 32;

// Pinout for DC
const int g_pwm_Pin = 25;
const int g_fg_Pin = 26;
const int g_fan_Power = 27;
const int g_frequency = 5000;
const int g_pwm_Channel = 0;
const int g_resolution = 8;

String g_md5_str = "";
String g_IP = "";
char g_chipId_String[17];
String ssid;

 bool espNowMode = false;
 bool toggleLED = false;
 bool htp_Wifi_Connected = false;
 unsigned long long toggleStartTime = 0;
 unsigned long long  buttonPressTime = 0;  
 unsigned long long  hotspotStartTime = 0;  
 unsigned long long  modeChangeInterval = 60000;  
const int BUTTON_PRESS_MIN_TIMER = 3000; 
const int BUTTON_PRESS_MAX_TIMER = 4000; 

clz_device_type_id g_device_Type = CLZ_APF_DC;
String g_subType = "";


void print_firmware_md5() {
    const esp_partition_t* running_partition = esp_ota_get_running_partition();
    if (!running_partition) {
        //Serial.println("Failed to get running partition");
        return;
    }

    // Buffer for reading data from flash
    uint8_t buffer[BUFFER_SIZE];
    size_t read_size;

    // Initialize MD5 context
    MD5Builder md5;
    md5.begin();

    // Read and hash the data in chunks
    size_t offset = 0;
    while (offset < running_partition->size) {
        read_size = (running_partition->size - offset > BUFFER_SIZE) ? BUFFER_SIZE : (running_partition->size - offset);
        esp_err_t err = esp_partition_read(running_partition, offset, buffer, read_size);
        if (err != ESP_OK) {
            //Serial.printf("Failed to read partition: %s\n", esp_err_to_name(err));
            return;
        }

        md5.add(buffer, read_size);
        offset += read_size;
    }

    // Finalize the MD5 hash
    md5.calculate();
    g_md5_str = md5.toString();

    // Print the MD5 hash
    Serial.print("Running firmware MD5: ");
    Serial.println(g_md5_str);
}


void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);

  delay(1000);
  Serial.println("Welcome to EVK-Purifier");
  Serial.println("Firmware Version : V1.00");
  print_firmware_md5();

  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, RTC_BROWNOUT_DET_LVL_2);

  uint64_t g_chipId = ESP.getEfuseMac();
  snprintf(g_chipId_String, 17, "%04X%08X", (uint16_t)(g_chipId >> 32), (uint32_t)g_chipId);

  pinMode(LED_PIN, OUTPUT);
  pinMode(WIFI_CONFIG_PIN, INPUT);
  pinMode(FILTER_COVER_PIN, INPUT);

  digitalWrite(LED_PIN, LOW);

  switch (g_device_Type) {
    case CLZ_APF_DC:
      Serial.println("DC Motor");
      WiFi.setTxPower(WIFI_POWER_11dBm);
      g_subType = "DC";
      pinMode(g_fg_Pin, INPUT);
      pinMode(g_fan_Power, OUTPUT);
      digitalWrite(g_fan_Power, LOW);
      ledcSetup(g_pwm_Channel, g_frequency, g_resolution);
      ledcAttachPin(g_pwm_Pin, g_pwm_Channel);
      delay(1000);
      ledcWrite(g_pwm_Channel, 0);
      delay(1000);
      break;

    case CLZ_APF_AC:
      g_subType = "AC";
      pinMode(HIGH_LED, OUTPUT);
      pinMode(MED_LED, OUTPUT);
      pinMode(LOW_LED, OUTPUT);
      break;

    case CLZ_PLG_AC:
      g_subType = "PLUG_AC";
      pinMode(PLG_LED, OUTPUT);
      pinMode(PLG_BLUE_LED, OUTPUT);
      pinMode(PLG_RED_LED, OUTPUT);
      break;

    default:
      break;
  }


uint8_t readEEPROM = EEPROM.read(0); 
Serial.print("readEEPROM: ");
Serial.println(readEEPROM);

int CLZ_DEVICE_MODE = 0; // (0 for hotspot, 1 for espnow)
if(readEEPROM == 255){
  CLZ_DEVICE_MODE = 1;
}
else{
  CLZ_DEVICE_MODE = 0; 
}

  if (CLZ_DEVICE_MODE == 1) {
    espNowMode = true;
    Serial.println("espnow mode");
    startESPNOW();
  } else {
    espNowMode = false;
    Serial.println("hotspot mode");
    startHotspot();
  }
}

void loop() {
  
  int filterStatus = (g_subType == "DC") ? digitalRead(FILTER_COVER_PIN) : true;

  if (espNowMode) {
    espnowFilterHandler(filterStatus);
  } 
  else {
    hotspotFilterHandler(filterStatus);

    if (millis() - toggleStartTime <= 60000) {
      toggleLED = (toggleLED == LOW) ? HIGH : LOW;
      digitalWrite(LED_PIN, toggleLED);
      delay(500);
    }
    else{
      if(htp_Wifi_Connected){
        digitalWrite(LED_PIN, HIGH);
        toggleStartTime = 0;
      }
      else{
        digitalWrite(LED_PIN, LOW);
      }
    }

    if (millis() - hotspotStartTime >= modeChangeInterval) {
        EEPROM.write(0, 255);
        EEPROM.commit();
        ESP.restart();  
    }
  }
  
  if(g_subType == "DC"){
    if (digitalRead(WIFI_CONFIG_PIN) == LOW) {
      if (buttonPressTime == 0) buttonPressTime = millis();

      if (millis() - buttonPressTime <= BUTTON_PRESS_MAX_TIMER && millis() - buttonPressTime >= BUTTON_PRESS_MIN_TIMER) {
        if (espNowMode) {
          EEPROM.write(0, 0);  // Save Hotspot mode in EEPROM
          EEPROM.commit();
          ESP.restart();
        } 
        else {
          EEPROM.write(0, 0);  // Save Hotspot mode in EEPROM
          EEPROM.commit();
          ESP.restart();
        }
      }
    } else {
      buttonPressTime = 0;  // Reset button press time if the button is released
    }
  }
}
