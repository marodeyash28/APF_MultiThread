#include "hotspot.h"

bool htp_Filter_Cover = false;
bool htp_Wifi_Pin_Status = false;
// bool htp_Wifi_Connected = false;
bool htp_LED_State = false; // Ask Nitish Sir its use
bool htp_Wifi_K_Status = true;
bool htp_Hold = false;  // Functionality overriden
int htp_UpdFirm = 0; // Always used as 0
int htp_Scanned_Devices = 0;
unsigned long long htp_Filter_TimerCurrent; // Functionality overriden
bool htp_Flickering_Done = false;  // Functionality overriden

QueueHandle_t commandQueue;


// Web server instance
WebServer server;

void handleCommand() {
  String command = server.arg("cmd");
  // Serial.println("Received command: " + command);
  if (xQueueSend(commandQueue, &command, portMAX_DELAY) != pdPASS) {
    // Serial.println("Failed to add command to queue.");
  }
  server.send(200, "text/plain", "Command received: " + command);
}

void set_http_server() {
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", "<h1>OTA Update Server</h1>");
  });

  server.on("/update", HTTP_POST, []() {
    server.send(200, "text/plain", "OTA Update received");
  }, []() {
    HTTPUpload& upload = server.upload();
    String errorMessage = "";

    if (upload.status == UPLOAD_FILE_START) {
      if (!Update.begin()) {
        errorMessage = "Failed to begin OTA update";
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        errorMessage = "Failed to write OTA update";
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (errorMessage.isEmpty()) {
        if (Update.end(true)) {
          server.send(200, "text/plain", "OTA Update Success");
          delay(5000);
          ESP.restart();
        } else {
          errorMessage = "Failed to end OTA update";
          Update.printError(Serial);
        }
      } else {
        server.send(500, "text/plain", "OTA Update Error: " + errorMessage);
      }
    }
  });

  server.onNotFound([]() {
    server.send(404, "text/plain", "Route Not Found");
  });

  server.begin();
}


void handleSaveWiFi() {
  if (server.hasArg("plain")) {
    String body = server.arg("plain");
    DynamicJsonDocument doc(200);
    deserializeJson(doc, body);
    String ssid = doc["ssid"];
    String password = doc["password"];
    WiFi.begin(ssid.c_str(), password.c_str());
    int trywifi = 0;
    while (WiFi.status() != WL_CONNECTED && trywifi<10) { 
      delay(1000);
      trywifi = trywifi+1;
    } 
   if (WiFi.status() == WL_CONNECTED){
    htp_Wifi_Pin_Status = true;
    htp_Wifi_Connected = true;
    delay(1000);
    digitalWrite(LED_PIN, HIGH);
    g_IP = WiFi.localIP().toString();  // Convert IPAddress to String
    String response = "<html><body><h1>WiFi Configured: " + g_IP + "</h1></body></html>";
    server.send(200, "text/html", response);
   }
   else{
    htp_Wifi_Pin_Status = false;
    htp_Wifi_Connected = false;
    //Serial.println("Unable to connect wifi network, Going back to hotspot");
    delay(1000);
    digitalWrite(LED_PIN, LOW);
    String response = "<html><body><h1>Wrong Configuration Plz Try Again</h1></body></html>";
    server.send(200, "text/html", response);
    g_IP= "";

   }
  }
}



void handleStatus() {
  String status = digitalRead(WIFI_CONFIG_PIN) == HIGH ? "ON" : "OFF";
  String response = "{\"io23\":\"" + status + "\"}";
  server.send(200, "application/json", response);
}


void handleRoot() {
  String ledStatus = htp_LED_State ? "ON" : "OFF";
  String statusColor = htp_LED_State ? "#4CAF50" : "#f44336"; 

  String commonHead = "<html>"
                      "<head>"
                      "<meta http-equiv=\"refresh\" content=\"30\">"
                      "<style>"
                      "body {"
                      "  font-family: Arial, sans-serif;"
                      "  text-align: center;"
                      "  background-color: #f5f5f5;"
                      "  margin: 0;"
                      "  padding: 0;"
                      "}"
                      "h1 {"
                      "  font-size: 32px;"
                      "  color: #444;"
                      "  margin: 20px 0;"
                      "}"
                      ".button {"
                      "  display: inline-block;"
                      "  margin: 10px;"
                      "  padding: 15px 30px;"
                      "  font-size: 18px;"
                      "  background-color: #1877F2;"
                      "  color: white;"
                      "  border: none;"
                      "  border-radius: 5px;"
                      "  cursor: pointer;"
                      "  transition: background-color 0.3s, transform 0.2s;"
                      "}"
                      ".button:hover {"
                      "  background-color: #145db2;"
                      "  transform: scale(1.05);"
                      "}"
                      ".button:active {"
                      "  transform: scale(0.98);"
                      "}"
                      ".container {"
                      "  display: flex;"
                      "  flex-direction: column;"
                      "  align-items: center;"
                      "  max-width: 600px;"
                      "  margin: 20px auto;"
                      "  padding: 40px;"
                      "  height: 30%;"
                      "  background-color: #fff;"
                      "  border: 1px solid #ddd;"
                      "  border-radius: 8px;"
                      "  box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1);"
                      "}"
                      ".row {"
                      "  display: flex;"
                      "  flex-wrap: wrap;"
                      "  justify-content: center;"
                      "  width: 100%;"
                      "  margin: 10px 0;"
                      "}"
                      "#status-box {"
                      "  width: 150px;"
                      "  padding: 12px;"
                      "  margin: 10px auto;"
                      "  text-align: center;"
                      "  border: 1px solid #ccc;"
                      "  border-radius: 8px;"
                      "  font-size: 18px;"
                      "  color: #fff;"
                      "}"
                      ".modal {"
                      "  display: none;"
                      "  position: fixed;"
                      "  z-index: 1;"
                      "  left: 0;"
                      "  top: 0;"
                      "  width: 100%;"
                      "  height: 100%;"
                      "  overflow: auto;"
                      "  background-color: rgba(0, 0, 0, 0.4);"
                      "  padding-top: 60px;"
                      "}"
                      ".modal-content {"
                      "  background-color: #fefefe;"
                      "  margin: 5% auto;"
                      "  padding: 20px;"
                      "  border: 1px solid #888;"
                      "  width: 80%;"
                      "  max-width: 500px;"
                      "  border-radius: 8px;"
                      "  position: relative;"
                      "}"
                      ".close {"
                      "  position: absolute;"
                      "  color: #aaa;"
                      "  top: 10px;"
                      "  float: right;"
                      "  font-size: 28px;"
                      "  font-weight: bold;"
                      "  cursor: pointer;"
                      "}"
                      ".close:hover,"
                      ".close:focus {"
                      "  color: black;"
                      "  text-decoration: none;"
                      "  cursor: pointer;"
                      "}"
                      ".hidden {"
                      "  display: none;"
                      "}"
                      "</style>"
                      "<script>"
                      "function toggleWiFiConfig() {"
                      "  let button = document.getElementById('toggleWiFiConfig');"
                      "  let currentState = button.innerText;"
                      "  let newState = (currentState === 'IO22-WiFi(ON)') ? 'IO22-WiFi(OFF)' : 'IO22-WiFi(ON)';"
                      "  button.innerText = newState;"
                      "  button.style.backgroundColor = (newState === 'IO22-WiFi(OFF)') ? '#1877F2' : '#145db2';"
                      "  sendCommand(newState);"
                      "  }"
                      "function getNetworks() {"
                      "  fetch('/scan-networks')"
                      "    .then(response => response.json())"
                      "    .then(data => {"
                      "      let ssidSelect = document.getElementById('ssid');"
                      "      ssidSelect.innerHTML = '';"
                      "      data.forEach(network => {"
                      "        let option = document.createElement('option');"
                      "        option.value = network.ssid;"
                      "        option.text = network.ssid;"
                      "        ssidSelect.appendChild(option);"
                      "      });"
                      "    });"
                      "} "
                      "function saveWiFiConfig() {"
                      "  let ssid = document.getElementById('ssid').value;"
                      "  let password = document.getElementById('password').value;"
                      "  fetch('/save-wifi', {"
                      "    method: 'POST',"
                      "    headers: { 'Content-Type': 'application/json' },"
                      "    body: JSON.stringify({ ssid, password })"
                      "  }).then(response => response.text())"
                      "    .then(text => {"
                      "      alert('Wi-Fi configuration saved!');"
                      "      toggleWiFiConfig();"
                      "    });"
                      "} "
                      "function sendCommand(cmd) {"
                      "  fetch('/command?cmd=' + cmd)"
                      "    .then(response => response.text())"
                      "    .then(text => {"
                      "      document.getElementById('status').innerText = 'Command Sent: ' + cmd;"
                      "    });"
                      "} "
                      "function toggleButton() {"
                      "  let button = document.getElementById('toggleButton');"
                      "  let currentState = button.innerText;"
                      "  let newState = (currentState === 'Test Mode (ON)') ? 'Test Mode (OFF)' : 'Test Mode (ON)';"
                      "  button.innerText = newState;"
                      "  button.style.backgroundColor = (newState === 'Test Mode (OFF)') ? '#1877F2' : '#145db2';"
                      "  sendCommand(newState);"
                      "} "
                      "function openModal() {"
                      "  if (confirm('Are you sure you want to upload the firmware?')) {"
                      "    sendCommand('UPFIRM');"
                      "    document.getElementById('myModal').style.display = 'block';"
                      "  }"
                      "} "
                      "function closeModal() {"
                      "  document.getElementById('myModal').style.display = 'none';"
                      "} "
                      "window.onclick = function(event) {"
                      "  let modal = document.getElementById('myModal');"
                      "  if (event.target == modal) {"
                      "    modal.style.display = 'none';"
                      "  }"
                      "} "
                      "</script>"
                      "</head>";

  String title;
  String buttonHtml;
  if (g_device_Type == CLZ_PLG_AC) {
    title = "<h1>PLUG HTML</h1>";
    buttonHtml = "<div class=\"row\">"
                 "  <button class=\"button\" onclick=\"sendCommand('On')\">ON</button>"
                 "  <button class=\"button\" onclick=\"sendCommand('Off')\">OFF</button>"
                 "</div>";
  } else {
    title = "<h1>PURIFIER HTML</h1>";
    buttonHtml = "<div class=\"row\">"
                 "  <button class=\"button\" onclick=\"sendCommand('Hi')\">HIGH</button>"
                 "  <button class=\"button\" onclick=\"sendCommand('Md')\">MED</button>"
                 "  <button class=\"button\" onclick=\"sendCommand('Lo')\">LOW</button>"
                 "  <button class=\"button\" onclick=\"sendCommand('Off')\">OFF</button>"
                 "</div>"
                 "<div class=\"row\">"
                 "  <div id=\"status-box\" style=\"background-color: " + String(htp_Wifi_Pin_Status ? "#1877F2" : "#f44336") + ";\">IO-21 " + String(htp_Wifi_Pin_Status ? "ON" : "OFF") + "</div>"
                 //"  <button class=\"button\" onclick=\"sendCommand('WiFi_Hit')\";\"toggleWiFiConfig()\">IO22(WiFi)</button>"
                //"  <button class=\"button\" onclick=\"sendCommand('WiFi_Hit')\">IO22(WiFi)</button>"
                "  <button id=\"toggleWiFiConfig\" class=\"button\" onclick=\"toggleWiFiConfig()\" style=\"background-color:" + String(htp_Wifi_K_Status ? "#1877F2" : "#145db2") + ";\">IO22-WiFi " + String(htp_Wifi_K_Status ? "(ON)" : "(OFF)") + "</button>"
                 "  <div id=\"status-box\" style=\"background-color: " + String(htp_Filter_Cover ? "#1877F2" : "#f44336") + ";\">IO-23 " + String(htp_Filter_Cover ? "ON" : "OFF") + "</div>"
                 "</div>";
  }
  String wifiConfigHtml = htp_Wifi_K_Status ? 
        "<div id=\"wifiConfig\">"
        "  <h2>WiFi Configuration</h2>"
        "  <select id=\"ssid\">" : "";  // Add an empty string if htp_Wifi_K_Status is false

  if (htp_Wifi_K_Status) { // Only scan networks if the WiFi config is visible
      int numNetworks = WiFi.scanNetworks();  // Scan for networks

      for (int i = 0; i < numNetworks; i++) {
          wifiConfigHtml += "<option value=\"" + WiFi.SSID(i) + "\">" + WiFi.SSID(i) + "</option>";
      }

    wifiConfigHtml += 
        "  </select>"
        "  <input id=\"password\" type=\"password\" placeholder=\"Password\" class=\"input-field\">"
        "  <div class=\"row\">"
        "    <button class=\"button\" onclick=\"saveWiFiConfig()\">Save</button>"
        "    <button class=\"button\" onclick=\"toggleWiFiConfig()\">Back</button>"
        "  </div>"
        "</div>";
  }

  String bodyHtml = "<body>" + title +
                    "<div class=\"container\">" + buttonHtml +
                    "<div class=\"row\" id=\"mainButtons\">" // Wrap main buttons in a div
                    "  <button id=\"toggleButton\" class=\"button\" onclick=\"toggleButton()\" style=\"background-color:" + String(htp_Hold ? "#1877F2" : "#145db2") + ";\">Test Mode " + String(htp_Hold ? "(ON)" : "(OFF)") + "</button>"
                    "  <button class=\"button\" onclick=\"openModal()\">Upload (Firm)</button>"
                    "</div>"
                    //"<div id=\"status-box\" style=\"background-color: #1877F2;\">WiFi IP : " + g_IP + "</div>"
                    //"<span><B>IP: " + g_IP + "</B></span>"
                    "<span style='font-size: 24px;'><b>IP: " + g_IP + "</b></span>"
                    "</div>"
                    "<div id=\"myModal\" class=\"modal\">"
                    "  <div class=\"modal-content\">"
                    "    <span class=\"close\" onclick=\"closeModal()\">&times;</span>"
                    "    <h2>Firmware Update</h2>"
                    "    <form id=\"updateForm\" method=\"POST\" action=\"/update\" enctype=\"multipart/form-data\">"
                    "      <input type=\"file\" name=\"file\" required>"
                    "      <input type=\"submit\" value=\"Upload Firmware\" class=\"button\">"
                    "    </form>"
                    "  </div>"
                    "</div>"
                    "</body>"
                    "</html>";

  server.send(200, "text/html", commonHead + bodyHtml + wifiConfigHtml);
}


void startHotspot() {
  EEPROM.write(0, 255);  // Save ESPNOW mode in EEPROM
  EEPROM.commit();
  ssid = "EVK-APF_" + String(g_chipId_String);           
  delay(1000); 
  WiFi.softAP(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.softAPIP());     
  
  server.on("/", handleRoot);    
  server.on("/command", handleCommand);
  server.on("/status", handleStatus);
  server.on("/save-wifi", HTTP_POST, handleSaveWiFi);
  
  set_http_server();
  commandQueue = xQueueCreate(10, sizeof(String));
  
  xTaskCreate(commandTask, "CommandTask", 4096, NULL, 3, NULL);
  xTaskCreate(clientTask, "ClientTask", 4096, NULL, 1, NULL);
}

void hotspotFilterHandler(int filterStatus) {
  // Handle filter status in hotspot mode
  if (filterStatus == HIGH) {
    htp_Filter_Cover = true;
    // Serial.println("Hotspot: Sensor is HIGH");
  } else {
    htp_Filter_Cover = false;
    // Serial.println("Hotspot: Sensor is LOW");
  }
}

void commandTask(void *pvParameters) {
  String receivedCommand;
  while (true) {
    if (htp_UpdFirm == 0){
      if (xQueueReceive(commandQueue, &receivedCommand, portMAX_DELAY) == pdPASS) {
        Serial.println(receivedCommand);
        if (receivedCommand=="Test Mode (OFF)"){
          //Serial.println("Hotspot released received");
        //   g_device_State= HOTSPOT;
            EEPROM.write(0, 255);
            EEPROM.commit();
            ESP.restart();
          htp_Hold = false;
        }
        else if(receivedCommand=="Test Mode (ON)"){
          //Serial.println("Hostpot hold received");
        //   g_device_State= HOTSPOT_HOLD;
            modeChangeInterval += 600000;
            htp_Hold = true;
        }
        else if(receivedCommand=="UPFIRM"){
          //Serial.println("Update Firmware received");
          //htp_UpdFirm = 1;
            modeChangeInterval += 600000;
        }
        else if(receivedCommand == "IO22-WiFi(ON)"){
          //Serial.println("IO22-WiFi(ON)....");
          //htp_UpdFirm = 1;
          htp_Wifi_K_Status= true;
            modeChangeInterval += 600000;
        //   flicker_wifiLed();
        }
        if(htp_Filter_Cover==true){
        control_h_speed(receivedCommand, 1);
        delay(1500); 
        }
        else{
          control_h_speed("Off", 1);
          delay(1500);
        }
      }
    }
  }
}

void clientTask(void *pvParameters) {
  while (true) {
    // htp_Filter_TimerCurrent = millis();
    if (htp_UpdFirm == 0){
      if(g_device_Type != CLZ_PLG_AC){
        // g_flag = digitalRead(WIFI_CONFIG_PIN);
        // if (STOP_FAN_STATE == 1){
        //     htp_Filter_Cover= true;
        //   }
        // else{
        //   htp_Filter_Cover= false;
        //   control_h_speed("Off", 1);
        //   }
        if(htp_Filter_Cover == false){
            control_h_speed("Off", 1);
        }
      }
      
      if(g_device_Type == CLZ_PLG_AC){
        htp_Filter_Cover = true;
      }
    int numDevices = WiFi.softAPgetStationNum();
    if (numDevices > htp_Scanned_Devices){
      //Serial.println("WIFI paring done");
      htp_Scanned_Devices = numDevices;
    }
    if (htp_Scanned_Devices > numDevices){
      //Serial.println(" WIFI paring terminated");
      // digitalWrite(WIFI_K, LOW);
       htp_Scanned_Devices = numDevices;
    } 
  } 
  //Serial.println(" Server client handling...");
  server.handleClient();
    delay(1500);   
}
}


void control_h_speed(String fanSpeed, int iState) {
    // modeChangeInterval += 600000;
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