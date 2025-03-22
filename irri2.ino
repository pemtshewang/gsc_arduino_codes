#include <WiFi.h>
#include <WebSocketsServer.h>
#include <PubSubClient.h>
#include <Wire.h>
#include "RTClib.h"
#include <Preferences.h>

#define RELAY_ON LOW
#define SOILMOISTURE_PIN 36  // Pin for soil moisture sensor Define pins for water valves
const int waterValvePins[] = {2, 4, 5, 12, 13, 14, 27};
const int numValves = sizeof(waterValvePins) / sizeof(int);
const int motorPin = 26;  // Pin for controlling the motor (if applicable)

// WiFi and MQTT credentials
const char* ssid = "Ye";
const char* password = "11111111";
const char* mqtt_server = "meb1e91e.ala.asia-southeast1.emqxsl.com";
const char* mqtt_username = "Yeshey";
const char* mqtt_password = "$2b$10$qsvUhTTpUS6NW4CLwJrjFOI931UWHANf76J5TyoH2ZPtEFTopccjW";
const int mqtt_port = 8883;

// MQTT broker IDs
static String controllerBrokerId = "6d3a1155-9";
static String userBrokerId = "YE72782384";

WiFiClientSecure espClient;
PubSubClient mqtt_client(espClient);
WebSocketsServer webSocket = WebSocketsServer(80);
RTC_DS3231 rtc;
Preferences prefs;

unsigned long lastMqttPublishTime = 0;

const char *ca_cert = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD
QTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IENBMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4jvhEXLeqKTTo1eqUKKPC3eQyaKl7hLOllsB
CSDMAZOnTjC3U/dDxGkAV53ijSLdhwZAAIEJzs4bg7/fzTtxRuLWZscFs3YnFo97
nh6Vfe63SKMI2tavegw5BmV/Sl0fvBf4q77uKNd0f3p4mVmFaG5cIzJLv07A6Fpt
43C/dxC//AH2hdmoRBBYMql1GNXRor5H4idq9Joz+EkIYIvUX7Q6hL+hqkpMfT7P
T19sdl6gSzeRntwi5m3OFBqOasv+zbMUZBfHWymeMr/y7vrTC0LUq7dBMtoM1O/4
gdW7jVg/tRvoSSiicNoxBN33shbyTApOB6jtSj1etX+jkMOvJwIDAQABo2MwYTAO
BgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUA95QNVbR
TLtm8KPiGxvDl7I90VUwHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I90VUw
DQYJKoZIhvcNAQEFBQADggEBAMucN6pIExIK+t1EnE9SsPTfrgT1eXkIoyQY/Esr
hMAtudXH/vTBH1jLuG2cenTnmCmrEbXjcKChzUyImZOMkXDiqw8cvpOp/2PV5Adg
06O/nVsJ8dWO41P0jmP6P6fbtGbfYmbW0W5BjfIttep3Sp+dWOIrWcBAI+0tKIJF
PnlUkiaY4IBIqDfv8NZ5YBberOgOzW6sRBc4L0na4UU+Krk2U886UAb3LujEV0ls
YSEY1QSteDwsOoBrp+uvFRTp2InBuThs4pFsiv9kuXclVzDAGySj4dzp30d8tbQk
CAUw7C29C79Fv1C5qfPrmAESrciIxpg0X40KPMbp1ZWVbd4=
-----END CERTIFICATE-----
)EOF";

// Soil moisture sensor calibration values
const int dryValue = 4095;  // Value for dry soil
const int wetValue = 1500;   // Value for wet soil

// Structure to store schedule information
struct Schedule {
  bool isActive;
  String startHour;
  String startMinute;
  String endHour;
  String endMinute;
  uint8_t repetitionDays;
};

Schedule valveSchedules[numValves]; // Array to store schedules for each valve

void saveScheduleToPrefs(int valveNum) {
  String scheduleKey = "valve_" + String(valveNum) + "_schedule";
  String scheduleData = String(valveSchedules[valveNum].isActive ? "true" : "false") + "|" +
                        String(valveSchedules[valveNum].startHour) + ":" + String(valveSchedules[valveNum].startMinute) + "|" +
                        String(valveSchedules[valveNum].endHour) + ":" + String(valveSchedules[valveNum].endMinute) + "|" +
                        String(valveSchedules[valveNum].repetitionDays);
  prefs.putString(scheduleKey.c_str(), scheduleData);
  Serial.print("Saved schedule for valve "); Serial.print(valveNum);
  Serial.print(":");
  Serial.println(scheduleData);
}

void scheduleValve(int valveNum, String startTime, String endTime, uint8_t repetitionDays) {
  Serial.println("Recieved time for scheduling "+startTime+"---"+endTime);
  valveSchedules[valveNum].isActive = true;
  
  // Parse start time
  int colonPos = startTime.indexOf(':');
  if (colonPos > 0) {
    valveSchedules[valveNum].startHour = startTime.substring(0, colonPos);
    valveSchedules[valveNum].startMinute = startTime.substring(colonPos+1);
    Serial.println("Stored timing for the start hour --->"+valveSchedules[valveNum].startHour+":"+valveSchedules[valveNum].startMinute);
    valveSchedules[valveNum].startMinute = startTime.substring(colonPos + 1);
  }
  
  // Parse end time
  colonPos = endTime.indexOf(':');
  if (colonPos > 0) {
    valveSchedules[valveNum].endHour = endTime.substring(0, colonPos);
    valveSchedules[valveNum].endMinute = endTime.substring(colonPos + 1);
  }
  
  valveSchedules[valveNum].repetitionDays = repetitionDays;
  Serial.println("Scheduled for: "+valveSchedules[valveNum].startHour);
  
  // Save the schedule to preferences
  saveScheduleToPrefs(valveNum);
  
  Serial.print("Scheduled valve ");
  Serial.print(valveNum);
  Serial.print(" from ");
  Serial.print(valveSchedules[valveNum].startHour);
  Serial.print(":");
  Serial.print(valveSchedules[valveNum].startMinute);
  Serial.print(" to ");
  Serial.print(valveSchedules[valveNum].endHour);
  Serial.print(":");
  Serial.print(valveSchedules[valveNum].endMinute);
}

// Check if the current time is within a schedule's time range
void checkScheduledIrrigations() {
  DateTime now = rtc.now();

  int currentDay = now.dayOfTheWeek();
  if(currentDay == 0) currentDay = 7;

  
  uint8_t dayMask = (1 << (currentDay - 1));  // Create bitmask for current day
  
  //check  the time 
  char currentTimeStr[9]; 
  sprintf(currentTimeStr, "%02d:%02d", now.hour(), now.minute());
  for (int i = 0; i < numValves; i++) {
    
    if (valveSchedules[i].isActive) {
      Serial.println("isActive");
      // Check if today is a scheduled day for this valve

      Serial.println(valveSchedules[i].repetitionDays);
      bool isScheduled = valveSchedules[i].repetitionDays & dayMask;
      Serial.println(isScheduled);
      if (!isScheduled) {
          Serial.println("isScheduled for now");
          String startTime = valveSchedules[i].startHour+":"+valveSchedules[i].startMinute;
          String endTime = valveSchedules[i].endHour+":"+valveSchedules[i].endMinute;
          Serial.println("--------s start time"+startTime+ "e current ---------"+currentTimeStr );
          if(startTime == currentTimeStr){
            Serial.println("The starttime matches as scheduled");
            digitalWrite(waterValvePins[i], RELAY_ON);
            digitalWrite(motorPin, RELAY_ON);
          }
          if(endTime == currentTimeStr){
            Serial.println("The endtime matches as scheduled");
            digitalWrite(waterValvePins[i], !RELAY_ON);
            digitalWrite(motorPin, !RELAY_ON);
          }
        } else {
          digitalWrite(waterValvePins[i], !RELAY_ON);
          // Only turn off motor if all valves are off
          bool anyValveOn = false;
          for (int j = 0; j < numValves; j++) {
            if (digitalRead(waterValvePins[j]) == RELAY_ON) {
              anyValveOn = true;
              break;
            }
          }
          if (!anyValveOn) {
            digitalWrite(motorPin, !RELAY_ON);
          }
        } 
      } 
  } 
}
void handleMqttMessage(char* topic, byte* payload, unsigned int length) {
  String receivedTopic = String(topic);
  String receivedPayload;
  for (int i = 0; i < length; i++) {
    receivedPayload += (char) payload[i];
  }
  Serial.print("Received MQTT message on topic: ");
  Serial.println(receivedTopic);
  Serial.print("Payload: ");
  Serial.println(receivedPayload);

  // Handle valve control messages
  if (receivedTopic.startsWith("user/" + userBrokerId + "/" + controllerBrokerId + "/actuator/valve/")) {
    int valveIndex = receivedTopic.substring(receivedTopic.lastIndexOf("/") + 1).toInt();
    if (valveIndex >= 0 && valveIndex < numValves) {
      if (receivedPayload == "open") {
        Serial.print("Opening valve ");
        Serial.println(valveIndex);
        digitalWrite(motorPin, RELAY_ON);
        digitalWrite(waterValvePins[valveIndex], RELAY_ON);
      } else if (receivedPayload == "close") {
        Serial.print("Closing valve ");
        Serial.println(valveIndex);
        digitalWrite(waterValvePins[valveIndex], !RELAY_ON);
        
        // Only turn off motor if all valves are off
        bool anyValveOn = false;
        for (int i = 0; i < numValves; i++) {
          if (digitalRead(waterValvePins[i]) == RELAY_ON) {
            anyValveOn = true;
            break;
          }
        }
        if (!anyValveOn) {
          digitalWrite(motorPin, !RELAY_ON);
        }
      }
    } else {
      Serial.println("Invalid valve index");
    }
  }

  // Handle scheduling messages
  else if (receivedTopic.startsWith("user/" + userBrokerId + "/" + controllerBrokerId + "/schedule")) {
    int valveIndex = receivedPayload.substring(0,1).toInt();

    valveIndex=valveIndex-1;

    if (valveIndex >= 0 && valveIndex < numValves) {
      // Parse the payload format: startTime|endTime|repetitionDays

      int firstDelimiter = receivedPayload.indexOf('|');
      int secondDelimiter = receivedPayload.indexOf('|',firstDelimiter+1);
      int lastDelimiter = receivedPayload.lastIndexOf("|");
      
      if (firstDelimiter > 0 && lastDelimiter > firstDelimiter) {
        String startTime = receivedPayload.substring(firstDelimiter+1, secondDelimiter);
        String endTime = receivedPayload.substring(secondDelimiter + 1, lastDelimiter);
        String repetitionDaysStr = receivedPayload.substring(lastDelimiter + 1);
        
        Serial.print("Scheduling valve ");
        Serial.print(valveIndex);
        Serial.print(" from ");
        Serial.print(startTime);
        Serial.print(" to ");
        Serial.print(endTime);
        
        scheduleValve(valveIndex, startTime, endTime, repetitionDaysStr.toInt());
      }
    } else {
      Serial.println("Invalid valve index");
    }
  }
}

void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_TEXT) {
    String message = (char*)payload;
    Serial.print("WebSocket message received: ");
    Serial.println(message);
    
    if (message.startsWith("valve:")) {
      String valveIndex = message.substring(6, message.indexOf(":", 6));
      String action = message.substring(message.indexOf(":", 6) + 1);
      int valveNum = valveIndex.toInt();
      
      Serial.print("Valve: ");
      Serial.print(valveNum);
      Serial.print(" Action: ");
      Serial.println(action);
      
      if (valveNum >= 0 && valveNum < numValves) {
        if (action == "open") {
          digitalWrite(motorPin, RELAY_ON);
          digitalWrite(waterValvePins[valveNum], RELAY_ON);
        } else if (action == "close") {
          digitalWrite(waterValvePins[valveNum], !RELAY_ON);
          
          // Only turn off motor if all valves are off
          bool anyValveOn = false;
          for (int i = 0; i < numValves; i++) {
            if (digitalRead(waterValvePins[i]) == RELAY_ON) {
              anyValveOn = true;
              break;
            }
          }
          if (!anyValveOn) {
            digitalWrite(motorPin, !RELAY_ON);
          }
        }
      }
    } 
    else if (message.startsWith("schedule:")) {
      String scheduleInfo = message.substring(9);
      int firstDelimiter = scheduleInfo.indexOf("|");
      int secondDelimiter = scheduleInfo.indexOf("|", firstDelimiter + 1);
      int thirdDelimiter = scheduleInfo.indexOf("|", secondDelimiter + 1);
      
      if (firstDelimiter > 0 && secondDelimiter > firstDelimiter && thirdDelimiter > secondDelimiter) {
        String valveIndex = scheduleInfo.substring(0, firstDelimiter);
        String startTime = scheduleInfo.substring(firstDelimiter + 1, secondDelimiter);
        String endTime = scheduleInfo.substring(secondDelimiter + 1, thirdDelimiter);
        String repetitionDaysStr = scheduleInfo.substring(thirdDelimiter + 1);
        
        int valveNum = valveIndex.toInt();
        uint8_t repetitionDayMask = 0;
        
        // Parse the repetition days string
        for (int i = 0; i < repetitionDaysStr.length(); i++) {
          char dayChar = repetitionDaysStr.charAt(i);
          if (dayChar >= '1' && dayChar <= '7') {
            int dayIndex = dayChar - '1';  // Convert '1' to 0, '2' to 1, etc.
            repetitionDayMask |= (1 << dayIndex);
          }
        }
        
        if (valveNum >= 0 && valveNum < numValves) {
          scheduleValve(valveNum, startTime, endTime, repetitionDayMask);
        }
      }
    }
  }
}

void setup() {
  Serial.begin(9600);
  
  // Initialize pins
  for (int i = 0; i < numValves; i++) {
    pinMode(waterValvePins[i], OUTPUT);
    digitalWrite(waterValvePins[i], !RELAY_ON);
  }

  pinMode(motorPin, OUTPUT);
  digitalWrite(motorPin, !RELAY_ON);
  
  // Setup WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  // Wait for WiFi connection
  int wifiAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && wifiAttempts < 20) {
    delay(500);
    Serial.print(".");
    wifiAttempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected to WiFi");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Failed to connect to WiFi");
  }
  
  // Setup Access Point
  WiFi.softAP("test", "testing");
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
  
  // Setup MQTT
  espClient.setCACert(ca_cert);
  mqtt_client.setKeepAlive(1200);
  mqtt_client.setServer(mqtt_server, mqtt_port);
  mqtt_client.setCallback(handleMqttMessage);
  
  //Setup RTC
  if (!rtc.begin()) {
    Serial.println("Could not find RTC!");
  } else {
    Serial.println("RTC found");
    // Uncomment this line to set the RTC to the date & time this sketch was compiled
    // rtc adjustment required for first time flash
    // uncomment and upload for second time
    // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // Wire.beginTransmission(0x68); // address DS3231
    // Wire.write(0x0E); // select register
    // Wire.write(0b00i011100); // write register bitmap, bit 7 is /EOSC
    // Wire.endTransmission();

    DateTime now = rtc.now();
    Serial.print("Current RTC time: ");
    Serial.print(now.year(), DEC);
    Serial.print('/');
    Serial.print(now.month(), DEC);
    Serial.print('/');
    Serial.print(now.day(), DEC);
    Serial.print(' ');
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.print(now.second(), DEC);
    Serial.print(" Day of week: ");
    Serial.println(now.dayOfTheWeek(), DEC);
  }
  
  // Setup preferences
  prefs.begin("irrigation_system", false);
  loadSchedulesFromPrefs();
  
  // Disable WiFi sleep mode for better connectivity
  WiFi.setSleep(false);
  
  // Start WebSocket server
  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);
  
  Serial.println("Setup complete");
  DateTime now = rtc.now();
  Serial.println(now.hour()+":"+now.minute());

}

void loop() {
  // Check and reconnect WiFi if needed
  if(WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, reconnecting...");
    WiFi.begin(ssid, password);
  } else {
    // Check and reconnect MQTT if needed
    if (!mqtt_client.connected()) {
      reconnect();
    }
    mqtt_client.loop();
  }
  
  webSocket.loop();
  checkScheduledIrrigations();
  
  delay(1000); // Small delay to prevent too frequent checking
}

void reconnect() {
  Serial.print("Attempting MQTT connection... ");
  if (!mqtt_client.connected()) {
    if (mqtt_client.connect("ESP32IrrigationClient", mqtt_username, mqtt_password)) {
      Serial.println("connected to the broker");
      mqtt_client.subscribe(("user/" + userBrokerId + "/" + controllerBrokerId + "/actuator/#").c_str());
      mqtt_client.subscribe(("user/" + userBrokerId + "/" + controllerBrokerId + "/schedule/#").c_str());
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt_client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void loadSchedulesFromPrefs() {
  for (int i = 0; i < numValves; i++) {
    String scheduleKey = "valve_" + String(i) + "_schedule";
    String scheduleData = prefs.getString(scheduleKey.c_str(), "");
    
    if (scheduleData.length() > 0) {
      Serial.print("Loading saved schedule for valve ");
      Serial.print(i);
      Serial.print(": ");
      Serial.println(scheduleData);
      
      int firstDelimiter = scheduleData.indexOf('|');
      int secondDelimiter = scheduleData.indexOf('|', firstDelimiter + 1);
      int thirdDelimiter = scheduleData.indexOf('|', secondDelimiter + 1);
      
      if (firstDelimiter > 0 && secondDelimiter > firstDelimiter && thirdDelimiter > secondDelimiter) {
        String isActiveStr = scheduleData.substring(0, firstDelimiter);
        String startTimeStr = scheduleData.substring(firstDelimiter + 1, secondDelimiter);
        String endTimeStr = scheduleData.substring(secondDelimiter + 1, thirdDelimiter);
        String repetitionDaysStr = scheduleData.substring(thirdDelimiter + 1);
        
        valveSchedules[i].isActive = (isActiveStr == "true");
        
        // Parse start time
        int colonPos = startTimeStr.indexOf(':');
        if (colonPos > 0) {
          valveSchedules[i].startHour = startTimeStr.substring(0, colonPos);
          valveSchedules[i].startMinute = startTimeStr.substring(colonPos + 1);
        }
        
        // Parse end time
        colonPos = endTimeStr.indexOf(':');
        if (colonPos > 0) {
          valveSchedules[i].endHour = endTimeStr.substring(0, colonPos);
          valveSchedules[i].endMinute = endTimeStr.substring(colonPos + 1);
        }
        
        // Parse repetition days
        valveSchedules[i].repetitionDays = repetitionDaysStr.toInt();
        
        Serial.print("Loaded schedule: Active=");
        Serial.print(valveSchedules[i].isActive);
        Serial.print(", Start=");
        Serial.print(valveSchedules[i].startHour);
        Serial.print(":");
        Serial.print(valveSchedules[i].startMinute);
        Serial.print(", End=");
        Serial.print(valveSchedules[i].endHour);
        Serial.print(":");
        Serial.print(valveSchedules[i].endMinute);
        Serial.print(", Days=");
        Serial.println(valveSchedules[i].repetitionDays, BIN);
      } else {
        valveSchedules[i].isActive = false;
      }
    } else {
      valveSchedules[i].isActive = false;
    }
  }
}
