#include "espnow.h"

unsigned long esp_Status_TimerCurrent = 0;
unsigned long esp_Status_TimerStart = 0;
unsigned long esp_Status_Timer = 60000;

bool esp_Device_Paired = false;
bool esp_Filter_Cover = false;
String esp_Prev_Fan_Speed = "";
String esp_Current_Fan_Speed = "Off";

int esp_Current_Ov_Flag = 0;
int esp_Current_State = 0;
int esp_Prev_Ov = 1;
int esp_Prvn = 0;

float esp_version = 0.0;
std::map<std::string, std::map<std::string, std::string>> esp_Peer_Sensors;

StaticJsonDocument<256> esp_Purifer_Status;
StaticJsonDocument<256> esp_My_Status;


unsigned char esp_Broadcast_Address[6] = {0xFF, 0xFF, 0xFF,0xFF, 0xFF, 0xFF};
QueueHandle_t esp_Espnow_Queue; 


void startESPNOW() {
  esp_Espnow_Queue = xQueueCreate(QUEUE_SIZE, sizeof(clz_espnow_event_t));

  WiFi.mode(WIFI_STA); 

  digitalWrite(LED_PIN, LOW);
  delay(1000);

  uint64_t g_chipId = ESP.getEfuseMac();
  snprintf(g_chipId_String, 17, "%04X%08X", (uint16_t)(g_chipId >> 32), (uint32_t)g_chipId);
  ssid = g_chipId_String;

  if (esp_now_init() == ESP_OK) {
    esp_now_register_recv_cb(espnow_recv_cb);
    esp_now_register_send_cb(espnow_send_cb);
    //Serialrintln("ESPNOW Init Succeed...");
  } 
  else {
    //Serialrintln("ESPNOW Init Failed...");
    delay(3000);
    ESP.restart();
  }

  esp_now_peer_info_t peerInfo = {};
  memcpy(&peerInfo.peer_addr, esp_Broadcast_Address, 6);

  if (!esp_now_is_peer_exist(esp_Broadcast_Address)) {
    esp_now_add_peer(&peerInfo);
  }
  
  xTaskCreate(espnow_task, "espnow_task", 8192, NULL, 4, NULL);

  publish_my_status();
}

void espnowFilterHandler(int filterStatus) {
  // Handle fliter status in espnow mode
  if (filterStatus == HIGH) {
    esp_Filter_Cover = true;
    // Serial.println("ESPNOW: Sensor is HIGH");
    if(esp_Prev_Fan_Speed != ""){
        //Serial.println("Condition Satiesfied and going to previous fan speed");
        esp_Current_Fan_Speed = esp_Prev_Fan_Speed;
        control_speed(esp_Prev_Fan_Speed, 1);
        esp_Prev_Fan_Speed = "";
      }
  } else {
    esp_Filter_Cover = false;
    // Serial.println("ESPNOW: Sensor is LOW");
    if(esp_Prev_Fan_Speed == ""){
        esp_Prev_Fan_Speed = esp_Current_Fan_Speed;        
        //Serial.print("Prev Fanspeed Set to Current Fan Speed =  ");
        //Serialrintln(esp_Prev_Fan_Speed);
        control_speed("Off", 1);
        delay(3000);
    }
  }
}

void publish_my_status() {
  esp_My_Status["type"] = "apf";
  esp_My_Status["req_type"] = "get_my_status";
  esp_My_Status["DeviceID"] = String(g_chipId_String);
  esp_My_Status["concept"] = g_subType;
  String statusData;
  serializeJson(esp_My_Status, statusData);

  clz_espnow_event_t _status_evt;
  _status_evt.id = ESPNOW_SEND_CB;
  _status_evt.data_len = statusData.length();
  _status_evt.data = (uint8_t *)malloc(_status_evt.data_len);
  _status_evt.sensorData = statusData;

  if (_status_evt.data != nullptr) {
    memcpy(_status_evt.data, statusData.c_str(), _status_evt.data_len);

    bool success = false;

    if (esp_Peer_Sensors.size() != 0) {
      for (const auto &peer : esp_Peer_Sensors) {
        uint8_t peer_mac[6];
        convertMacAddress(String(peer.first.c_str()), peer_mac);
        if (!sendToQueue(peer_mac, &_status_evt)) {
          //Serial.println("Send queue failed for peer: ");
          //Serial.println(peer.first.c_str());
        }
      }
      success = true;
    } else {
      success = sendToQueue(esp_Broadcast_Address, &_status_evt);
    }

    if (!success) {
      //Serial.println("Send queue failed");
      ESP.restart();
    }

    free(_status_evt.data);
  } else {
    //Serial.println("Failed to allocate memory for status data");
  }
}


bool isPeerPresent(const String &temp) {
  return esp_Peer_Sensors.find(temp.c_str()) != esp_Peer_Sensors.end();
}


void removePeer(const uint8_t *peer_addr) {
  char _macStr[18];
  formatMacAddress(peer_addr, _macStr, sizeof(_macStr));

  if (esp_now_is_peer_exist(peer_addr)) {
    esp_err_t result = esp_now_del_peer(peer_addr);
    if (result == ESP_OK) {
      auto it = esp_Peer_Sensors.find(_macStr);
      if (it != esp_Peer_Sensors.end()) {
        esp_Peer_Sensors.erase(it);
        //Serial.print("Successfully Removed peer sensor: ");
        //Serial.println(_macStr);
      } else {
        //Serial.println("Peer sensor not found in collection");
      }
      delay(5000);
    } else {
      //Serial.print("Peer removal failed with error: ");
      //Serial.println(result);
    }
  } else {
    //Serial.println("Peer does not exist");
  }
}


void espnow_task(void *pvParameter) {
  clz_espnow_event_t _task_evt;

  while (xQueueReceive(esp_Espnow_Queue, &_task_evt, portMAX_DELAY) == pdTRUE) {
    esp_Status_TimerStart = esp_Status_TimerCurrent;
    esp_Status_TimerCurrent = millis();
    if (esp_Status_TimerCurrent - esp_Status_TimerStart >= esp_Status_Timer) {
      send_apf_status(_task_evt.mac_addr, MULTICAST);
    //   Serial.println(
    //       "60 Second counter success. Sending out APF status to all Peers "
    //       "using Multicast.");
    }

    switch (_task_evt.id) {
      case ESPNOW_RECV_CB: {
        char buffer[ESP_NOW_MAX_DATA_LEN + 1];
        int msgLen = min(ESP_NOW_MAX_DATA_LEN, _task_evt.data_len);
        strncpy(buffer, (const char *)_task_evt.data, msgLen);
        buffer[msgLen] = '\0';
        char macStr[18];
        formatMacAddress(_task_evt.mac_addr, macStr, 18);

        const size_t bufferSize = JSON_OBJECT_SIZE(8) + 140;
        StaticJsonDocument<bufferSize> doc;
        DeserializationError error = deserializeJson(doc, buffer);

        if (error) {
          //Serial.print("deserializeJson() failed: ");
          //Serial.println(error.c_str());
        } else {
          String _deviceID = doc["DeviceID"];
          String _fanSpeed = doc["Fan_Speed"];
          int _state = doc["State"];

          char temp[18];
          formatMacAddress(_task_evt.mac_addr, temp, 18);

          if (esp_Device_Paired && !esp_Peer_Sensors.empty() && isPeerPresent(String(temp))) {
            // Serial.printf(
            //     "espnow_task-if- Paired Meter--ESPNOW_RECV_CB: %s - %s\n",
            //     macStr, buffer);

            if (doc.containsKey("type")) {
              //Serial.println("In Doc Type");
              String type = doc["type"];

              if(type == "all_device_reset"){
                digitalWrite(LED_PIN, LOW);
                control_speed("Off", 1);
                ESP.restart();
              }

              if (type == "device_reset" ) {
                JsonArray namesArray = doc["devices"];

                for (const JsonVariant &name : namesArray) {
                  if (strcmp(name.as<const char *>(), g_chipId_String) == 0) {
                    removePeer(_task_evt.mac_addr);
                    if (esp_Peer_Sensors.size() == 0) {
                       esp_Device_Paired = false;
                       digitalWrite(LED_PIN, LOW);
                       control_speed("Off", 1);
                    }
                  }
                }
              }

              if(type == "update_firmware"){
                JsonArray namesArray = doc["devices"];
                for (const JsonVariant &name : namesArray) {
                  if (strcmp(name.as<const char *>(), g_chipId_String) == 0) {
                    // Serial.println("Inside Update firmware call...");
                    g_Update_Firmware = true;
                  }
                }
              }
            }

            if (doc.containsKey("ov_flag")) {
              esp_Current_Ov_Flag = doc["ov_flag"];
              //Serial.println("In esp_Current_Ov_Flag Type");
              esp_Current_State = _state;
            }

            if (doc.containsKey("Fan_Speed")) {
              if (esp_Current_Ov_Flag == 0) {  
                // Serial.println("Auto Mode...");
                if (esp_Prev_Ov == 0) {
                  esp_Peer_Sensors[macStr]["fanSpeed"] = _fanSpeed.c_str();
                  esp_Peer_Sensors[macStr]["timestamp"] =
                      std::to_string(esp_Status_TimerCurrent);

                  // Serial.printf("In Auto Mode _fanSpeed : %s\n",
                  //               _fanSpeed.c_str());
                  calculateFanSpeed(_fanSpeed, _state);
                } else {
                  esp_Prev_Ov = 0;
                }

                esp_Current_Fan_Speed = _fanSpeed.c_str();
                esp_Current_State = _state;

              } else {
                // Serial.println("Manual Mode...");
                if (_deviceID == String(g_chipId_String)) {
                  // Serial.printf("In Manual Mode _fanSpeed : %s\n",
                  //               _fanSpeed.c_str());
                  esp_Prev_Ov = 1;
                  esp_Current_Fan_Speed = _fanSpeed.c_str();
                  esp_Current_State = _state;
                }
              }
              if(esp_Filter_Cover == false ){
                if(esp_Prev_Fan_Speed == ""){
                  esp_Prev_Fan_Speed = esp_Current_Fan_Speed;
                  //Serial.print("Prev Fanspeed: ");
                  //Serial.println(esp_Prev_Fan_Speed);
                }
               esp_Current_Fan_Speed = "Off";
              }
              else{
                if(esp_Prev_Fan_Speed != ""){
                  esp_Current_Fan_Speed = esp_Prev_Fan_Speed;
                  esp_Prev_Fan_Speed = "";
                }
              }
              control_speed(esp_Current_Fan_Speed, esp_Current_State);

              //Serial.println("Sending APF Status");
              send_apf_status(_task_evt.mac_addr, MULTICAST);
            }
          } else if (esp_Device_Paired && !esp_Peer_Sensors.empty() &&
                     !isPeerPresent(temp)) {
            if (esp_Peer_Sensors.size() <= 1) {
              // if(g_Unlock == 1){
                if (doc.containsKey("type")) {
                  String type = doc["type"];
                  if (type == "scan") {
                    //Serial.printf("espnow_task-else if-ESPNOW_RECV_CB from Unpaired Peer: ""%s - %s\n",macStr, buffer);
                    esp_now_peer_info_t _peerInfo = {};
                    memcpy(_peerInfo.peer_addr, _task_evt.mac_addr, 6);

                    if (!esp_now_is_peer_exist(_task_evt.mac_addr)) {
                      // Serial.println("Adding Peer.-- In RECV_CB else if
                      // condition, delay 5 seconds. -- TEMP_UNICAST");
                      esp_now_add_peer(&_peerInfo);
                    } else {
                      // Serial.println(
                      //     "Error!!! -- This Peer should not be in the Peer List");
                    }
                    delay(500);
                    send_apf_status(_task_evt.mac_addr, TEMP_UNICAST);
                  }
                }
                if (doc.containsKey("Provisioned_devices")) {
                  //Serial.printf(
                    //   "Inside espnow_task. else if condition -- -- Different "
                    //   "Peer exists. Unpaired Meter -- Scan or Provision. "
                    //   "Received message from: %s - %s\n",
                    //   macStr, buffer);
                  JsonArray namesArray = doc["Provisioned_devices"];

                  for (const JsonVariant &name : namesArray) {
                    if (strcmp(name.as<const char *>(), g_chipId_String) == 0) {
                      // g_Unlock = 0;
                      addPeer(_task_evt.mac_addr);
                      //AJ 09.21
                      digitalWrite(LED_PIN, HIGH);
                    }
                  }
                }
              // }
              else{
                // //Serial.print("g_Unlock is: ");
                // //Serial.println(g_Unlock);
              }
            } else {
              //Serial.println("Peer size reached its limit...");
            }
          } else {
            if (doc.containsKey("type")) {
              String type = doc["type"];

              if (type == "scan") {
                // Serial.printf(
                //     "espnow_task-else- Unpaired Meter call--ESPNOW_RECV_CB "
                //     "from: %s - %s\n",
                //     macStr, buffer);
                esp_now_peer_info_t _peerInfo = {};
                memcpy(_peerInfo.peer_addr, _task_evt.mac_addr, 6);

                if (!esp_now_is_peer_exist(_task_evt.mac_addr)) {
                  // //Serial.println("Adding Peer.-- Prior to sendToQueue
                  // TEMP_UNICAST");
                  esp_now_add_peer(&_peerInfo);
                } else {
                  // //Serial.println(
                  //     "Error!!! -- This Peer should not be in the Peer List");
                }

                delay(500);
                send_apf_status(_task_evt.mac_addr, TEMP_UNICAST);
              }
            }
            if (doc.containsKey("Provisioned_devices")) {
            //   Serial.printf(
            //       "Inside espnow_task. else condition -- -- No Peers. Unpaired "
            //       "Meter -- Scan or Provision. Received message from: %s - "
            //       "%s\n",
            //       macStr, buffer);
              JsonArray namesArray = doc["Provisioned_devices"];
              for (const JsonVariant &name : namesArray) {
                if (strcmp(name.as<const char *>(), g_chipId_String) == 0) {
                  // //Serial.println(g_chipId_String);
                  esp_Device_Paired = true;
                  // g_Unlock = 0;
                  addPeer(_task_evt.mac_addr);
                  digitalWrite(LED_PIN, HIGH);
                }
              }
            }
          }
        }
        free(_task_evt.data);
        break;
      }
      case ESPNOW_SEND_CB: {
        size_t messageLength = _task_evt.sensorData.length();
        uint8_t messageBuffer[messageLength + 1];  // +1 for null terminator
        _task_evt.sensorData.getBytes(messageBuffer, messageLength + 1);

        broadcast_on_espnow(messageBuffer, messageLength, _task_evt.mac_addr,_task_evt.broadcast_mode);
        esp_Status_TimerStart = esp_Status_TimerCurrent;
        
        break;
      }
      case ESPNOW_CLZ_DEL_PEER: {
        char _macStr[18];
        formatMacAddress(_task_evt.mac_addr, _macStr, 18);
        if (esp_now_is_peer_exist(_task_evt.mac_addr)) {
            esp_now_del_peer(_task_evt.mac_addr);
            //Serial.print("Peer removed: ");
            //Serial.println(_macStr);
        } 
        else {
            //Serial.print("Error!!! Peer not found: ");
            //Serial.println(_macStr);
        }

        break;
      }
      default: {
        //Serial.print("Callback type error: ");
        //Serial.println(_task_evt.id);
        break;
      }
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void addPeer(const uint8_t *peer_addr) {
  char _macStr[18];
  formatMacAddress(peer_addr, _macStr, 18);

  // Add to Peer list
  if (!esp_now_is_peer_exist(peer_addr)) {
    esp_now_peer_info_t peerInfo = {};

    memcpy(peerInfo.peer_addr, peer_addr, ESP_NOW_ETH_ALEN);
    esp_err_t result = esp_now_add_peer(&peerInfo);

    if (result == ESP_OK) {
      //Serial.printf("Peer added successfully with MACID = ... %s \n", _macStr);
    } else {
      //Serial.print("Failed to add peer with error: ");
      //Serial.println(result);
    }
  } else {
    //Serial.println("Peer already exists");
  }

  // Add to collection

  if (_macStr != "ff:ff:ff:ff:ff:ff") {
    esp_Peer_Sensors[_macStr]["fanSpeed"] = "Lo";  // change
    esp_Peer_Sensors[_macStr]["timestamp"] = esp_Status_TimerCurrent;
  }
}

void broadcast_on_espnow(const uint8_t *messageBuffer, size_t messageLength,
                         const uint8_t *iMacAddress, clz_espnow_broadcast_mode iBroadcast_Mode) {
    esp_err_t result = esp_now_send(iMacAddress, messageBuffer, messageLength);

    if (result == ESP_OK) {
        // Serial.println("Data sent successfully in broadcast_on_espnow.");
        
        if (iBroadcast_Mode == TEMP_UNICAST) {
            clz_espnow_event_t _temp_uni;
            memset(&_temp_uni, 0, sizeof(_temp_uni));
            _temp_uni.id = ESPNOW_CLZ_DEL_PEER;
            memcpy(_temp_uni.mac_addr, iMacAddress, 6);

            if (!sendToQueue(iMacAddress, &_temp_uni)) {
                //Serial.println("Failed to send _temp_uni to queue.");
            }
        }
    } else {
       //Serial.print("Error sending data: ");
        //Serial.println(esp_err_to_name(result)); 
    }
    
    delay(2000);
}

bool sendToQueue(const uint8_t *mac_addr, clz_espnow_event_t *event) {
  memcpy(event->mac_addr, mac_addr, 6);

  char _macStr[18];
  formatMacAddress(mac_addr, _macStr, 18);

  if (xQueueSend(esp_Espnow_Queue, event, portMAX_DELAY) != pdTRUE) {
    //Serial.println("Send queue failed");
    free(event->data);
    return false;
  }
  return true;
}

void calculateFanSpeed(String &_fanSpeed, int &iState) {

  bool isHigh = false;
  bool isMedium = false;
  bool isLow = false;
  bool isOff = false;

  for (auto &peer : esp_Peer_Sensors) {
    std::map<std::string, std::string> &sensorData = peer.second;

    auto timeIt = sensorData.find("timestamp");
    if (timeIt != sensorData.end()) {
      int time = safeStringToInt(timeIt->second);

      if (esp_Status_TimerCurrent - time > 120000) {
        sensorData["fanSpeed"] = "Off";
         //Serial.printf(
        //     "Sensor %s: Timestamp outdated, setting fanSpeed to Off\n",
        //     peer.first.c_str());
      }
    } else {
      //Serial.println("Timestamp not found for sensor");
    }

    auto fanSpeedIt = sensorData.find("fanSpeed");
    if (fanSpeedIt != sensorData.end()) {
      const std::string &fanSpeed = fanSpeedIt->second;
      if (fanSpeed == "Hi") {
        isHigh = true;
      } else if (fanSpeed == "Md") {
        isMedium = true;
      } else if (fanSpeed == "Lo") {
        isLow = true;
      } else if (fanSpeed == "Off") {
        isOff = true;
      }
    } else {
      //Serial.println("Fan Speed: Not found");
    }
  }

  if (isHigh) {
    _fanSpeed = "Hi";
    iState = 1;
  } else if (isMedium) {
    _fanSpeed = "Md";
    iState = 1;
  } else if (isLow) {
    _fanSpeed = "Lo";
    iState = 1;
  } else if (isOff) {
    _fanSpeed = "Off";
    iState = 0;
  }
}

int safeStringToInt(const std::string &str) {
  try {
    return std::stoi(str);
  } catch (const std::invalid_argument &e) {
    //Serial.println("Invalid argument in string to int conversion.");
    return 0;  // Or some default value
  } catch (const std::out_of_range &e) {
    //Serial.println("Out of range in string to int conversion.");
    return 0;  // Or some default value
  }
}

void convertMacAddress(String macStr, uint8_t *macArray) {
  char *token = strtok(const_cast<char *>(macStr.c_str()), ":");
  for (int i = 0; i < 6; i++) {
    macArray[i] = strtol(token, NULL, 16);
    token = strtok(NULL, ":");
  }
}


void send_apf_status(const uint8_t *mac_addr, clz_espnow_broadcast_mode iMode) {
  char _macStr[18];
  formatMacAddress(mac_addr, _macStr, 18);

  esp_Purifer_Status["DeviceID"] = String(g_chipId_String);
  esp_Purifer_Status["type"] = "apf";
  esp_Purifer_Status["STATE"] = esp_Current_State;
  esp_Purifer_Status["fanSpeed"] = esp_Current_Fan_Speed;
  esp_Purifer_Status["auto"] = esp_Current_Ov_Flag;
  esp_Purifer_Status["concept"] = g_subType;
  esp_Purifer_Status["filter"] = esp_Filter_Cover;
  esp_Purifer_Status["prvn"] = esp_Peer_Sensors.size();
  esp_Purifer_Status["VER"] = String(esp_version, 1); 

  String _purifier_Status;
  serializeJson(esp_Purifer_Status, _purifier_Status);

  clz_espnow_event_t _apf_evt;
  _apf_evt.id = ESPNOW_SEND_CB;
  _apf_evt.data_len = _purifier_Status.length();
  _apf_evt.data = (uint8_t *)malloc(_apf_evt.data_len);
  _apf_evt.sensorData = _purifier_Status;

  if (_apf_evt.data != nullptr) {
    memcpy(_apf_evt.data, _purifier_Status.c_str(), _apf_evt.data_len);
    memcpy(_apf_evt.mac_addr, mac_addr, 6);

    bool result = false;

    switch (iMode) {
      case UNICAST: {
        _apf_evt.broadcast_mode = UNICAST;
        result = sendToQueue(_apf_evt.mac_addr, &_apf_evt);
        break;
      }

      case MULTICAST: {
        _apf_evt.broadcast_mode = MULTICAST;

        for (const auto &peer : esp_Peer_Sensors) {
          uint8_t peer_mac[6];
          convertMacAddress(String(peer.first.c_str()), peer_mac);
          if (!sendToQueue(peer_mac, &_apf_evt)) {
            //Serial.print("Send queue failed for MULTICAST to peer: ");
            //Serial.println(peer.first.c_str());
          }
          delay(500);
          //Serial.print("APF Status sent to Queue via Multicast : ");
          //Serial.println(peer.first.c_str());
        }
        result = true;
        break;
      }

      case TEMP_UNICAST: {
        _apf_evt.broadcast_mode = TEMP_UNICAST;

        uint8_t peer_mac[6];
        convertMacAddress(_macStr, peer_mac); 

        if (!sendToQueue(peer_mac, &_apf_evt)) {
            //Serial.print("Send queue failed for TEMP_UNICAST to peer: ");
            //Serial.println(_macStr);
        }
        
        delay(500);

        //Serial.print("APF Status sent to Queue via TEMP_UNICAST: ");
        //Serial.println(_macStr);
        
        result = true;
        break;
      }


      case BROADCAST: {
        _apf_evt.broadcast_mode = BROADCAST;
        result = sendToQueue(esp_Broadcast_Address, &_apf_evt);
        //Serial.print("APF Status sent via Queue to BROADCAST: ");

        break;
      }

      default: {
         //Serial.println("Error Condition.");
        break;
      }
    }

    if (!result) {
      //Serial.print("Could not send APF Status to Queue. ");
      ESP.restart();
    }
    else{
      //Serial.print("APF Status Sent: ");
      //Serial.println(_purifier_Status);
    }
    free(_apf_evt.data);
  } else {
    //Serial.println("Failed to allocate memory for event data");
  }
}

void formatMacAddress(const uint8_t *mac_addr, char *buffer, int maxLength) {
  snprintf(buffer, maxLength, "%02x:%02x:%02x:%02x:%02x:%02x", mac_addr[0],
           mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
}


void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status) {
   //Serial.print("Last Packet Send Status: ");

  if (status == ESP_NOW_SEND_SUCCESS) {
    //Serial.println("ESP_NOW_SEND_SUCCEED -- Send Succeed");
  } else if (status == ESP_NOW_SEND_FAIL) {
     //Serial.println("ESP_NOW_SEND_FAIL -- Send Failed");
  } else {
     //Serial.println("Unknown Error");
  }
}

void espnow_recv_cb(const uint8_t *i_mac_addr, const uint8_t *data, int len) {
  clz_espnow_event_t _recv_cb_evt;
  _recv_cb_evt.id = ESPNOW_RECV_CB;
  memcpy(_recv_cb_evt.mac_addr, i_mac_addr, 6);
  _recv_cb_evt.data = (uint8_t *)malloc(len);

  if (_recv_cb_evt.data == NULL) {
    //Serial.println("Empty Data Received. Malloc receive data fail");
    return;
  } else {
    _recv_cb_evt.data_len = len;
    memcpy(_recv_cb_evt.data, data, len);
  }

  if (xQueueSend(esp_Espnow_Queue, &_recv_cb_evt, portMAX_DELAY) != pdTRUE) {
    // //Serial.println(
    //    "Could not add to the Queue. Queue failed in... espnow_recv_cb");
    // //Serial.print("Restarting ESP...");
    free(_recv_cb_evt.data);
    ESP.restart();
  }
}


void control_speed(String fanSpeed, int iState) {
  if (iState == 0) {
    // //Serial.print("Inside control__speed function. iState = 0. FanSpeed = "); 
    //Serial.println(fanSpeed);     
    switch (g_device_Type) {
      case CLZ_APF_DC:
        // //Serial.println("Device Type : DC");
        digitalWrite(g_fan_Power, HIGH);
        ledcWrite(g_pwm_Channel, 0);
        break;
      case CLZ_APF_AC:
        // //Serial.println("Device Type : AC");
        digitalWrite(HIGH_LED, LOW);
        digitalWrite(MED_LED, LOW);
        digitalWrite(LOW_LED, LOW);
        break;
      case CLZ_PLG_AC:
        // //Serial.println("Device Type : PLG");
        digitalWrite(PLG_LED, LOW);
        digitalWrite(PLG_BLUE_LED, LOW);
        digitalWrite(PLG_RED_LED, HIGH);
        break;
      default:
        // //Serial.println("Device Type : Not Found");
        break;
    }
    return;
  }

  if (iState == 1) {
    // //Serial.print("Inside control__speed function. iState = 1. FanSpeed = ");
    // //Serial.println(fanSpeed);    
    switch (g_device_Type) {
      case CLZ_APF_DC:
        // //Serial.println("Device Type : DC");
        digitalWrite(g_fan_Power, HIGH);
        if (fanSpeed == "Hi") {
          ledcWrite(g_pwm_Channel, 240);
        } else if (fanSpeed == "Md") {
          ledcWrite(g_pwm_Channel, 179);
        } else if (fanSpeed == "Lo") {
          ledcWrite(g_pwm_Channel, 128);
        } else if (fanSpeed == "Off") {
          ledcWrite(g_pwm_Channel, 0);
        }
        break;
      case CLZ_APF_AC:
        // //Serial.println("Device Type : AC");
        if (fanSpeed == "Hi") {
          digitalWrite(HIGH_LED, HIGH);
          digitalWrite(MED_LED, LOW);
          digitalWrite(LOW_LED, LOW);
        } else if (fanSpeed == "Md") {
          digitalWrite(HIGH_LED, LOW);
          digitalWrite(MED_LED, HIGH);
          digitalWrite(LOW_LED, LOW);
        } else if (fanSpeed == "Lo") {
          digitalWrite(HIGH_LED, LOW);
          digitalWrite(MED_LED, LOW);
          digitalWrite(LOW_LED, HIGH);
        } else if (fanSpeed == "Off") {
          digitalWrite(HIGH_LED, LOW);
          digitalWrite(MED_LED, LOW);
          digitalWrite(LOW_LED, LOW);
        }
        break;
      case CLZ_PLG_AC:
        // //Serial.println("Device Type : PLG_AC");
        if (fanSpeed == "Hi" || fanSpeed == "Md" || fanSpeed == "Lo" || fanSpeed == "On") {
          digitalWrite(PLG_LED, HIGH);
          digitalWrite(PLG_BLUE_LED, HIGH);
          digitalWrite(PLG_RED_LED, LOW);
        } else if (fanSpeed == "Off") {
          digitalWrite(PLG_LED, LOW);
          digitalWrite(PLG_BLUE_LED, LOW);
          digitalWrite(PLG_RED_LED, HIGH);
        }
        break;
      default:
        //Serial.println("Device Type : Not Found");
        break;
    }
  }
}