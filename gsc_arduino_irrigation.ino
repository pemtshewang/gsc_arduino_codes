#include <WiFi.h>
#include <WebSocketsServer.h>
#include <PubSubClient.h>
#include <Wire.h>
#include "RTClib.h"
#include <Preferences.h>

#define RELAY_ON LOW
#define SOILMOISTURE_PIN 36  // Pin for soil moisture sensor

// Define pins for water valves
const int waterValvePins[] = {2, 4, 5, 12, 13, 14, 27};
const int numValves = sizeof(waterValvePins) / sizeof(int);
const int motorPin = 26;  // Pin for controlling the motor (if applicable)

// WiFi and MQTT credentials
const char* ssid = "welovewifi";
const char* password = "noteasypassword";
const char* mqtt_server = "meb1e91e.ala.asia-southeast1.emqxsl.com";
const char* mqtt_username = "Deki wangmo";
const char* mqtt_password = "$2b$10$Gi1vRWSSfQXwON7pNzVAy.b3.uxc4PEoK0FitMLGV4iGjgGvK2gZa";
const int mqtt_port = 8883;

// MQTT broker IDs
static String controllerBrokerId = "85a2afbd-d";
static String userBrokerId = "DE38472920";

WiFiClientSecure espClient;
PubSubClient mqtt_client(espClient);
WebSocketsServer webSocket = WebSocketsServer(80);
RTC_DS3231 rtc;
Preferences prefs;

unsigned long lastMqttPublishTime = 0;

const char* ca_cert = R"EOF(
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
  DateTime startTime;
  DateTime endTime;
  uint8_t repetitionDays;
};

Schedule valveSchedules[numValves]; // Array to store schedules for each valve

void scheduleValve(int valveNum, String startTime, String endTime, uint8_t repetitionDays) {
  valveSchedules[valveNum].isActive = true;
  valveSchedules[valveNum].startTime = DateTime(rtc.now().unixtime() + parseTimeString(startTime));
  valveSchedules[valveNum].endTime = DateTime(rtc.now().unixtime() + parseTimeString(endTime));
  valveSchedules[valveNum].repetitionDays = repetitionDays;
}

unsigned long parseTimeString(String timeStr) {
  unsigned long hours = timeStr.substring(0, 2).toInt();
  unsigned long minutes = timeStr.substring(3, 5).toInt();
  return (hours * 3600UL) + (minutes * 60UL);
}

void checkScheduledIrrigations() {
  DateTime now = rtc.now();
  for (int i = 0; i < numValves; i++) {
    if (valveSchedules[i].isActive) {
      if ((valveSchedules[i].repetitionDays & (1 << (now.dayOfTheWeek() - 1))) > 0) { 
        if (now >= valveSchedules[i].startTime && now < valveSchedules[i].endTime) {
          digitalWrite(waterValvePins[i], RELAY_ON);
        } else if (now >= valveSchedules[i].endTime) {
          digitalWrite(waterValvePins[i], !RELAY_ON);
          valveSchedules[i].isActive = false;
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
  Serial.println(topicStr);
  Serial.print("Payload: ");
  Serial.println(receivedPayload);

  // Handle valve control messages
  if (topicStr.startsWith("user/" + userBrokerId + "/" + controllerBrokerId + "/actuator/valve/")) {
    int valveIndex = topicStr.substring(topicStr.lastIndexOf("/") + 1).toInt();
    if (valveIndex >= 0 && valveIndex < numValves) {
      if (receivedPayload == "open") {
        Serial.print("Opening valve ");
        Serial.println(valveIndex);
        digitalWrite(motorPin, RELAY_ON);
        digitalWrite(waterValvePins[valveIndex], RELAY_ON);
      } else if (receivedPayload == "close") {
        Serial.print("Closing valve ");
        Serial.println(valveIndex);
        digitalWrite(motorPin, !RELAY_ON);
        digitalWrite(waterValvePins[valveIndex], !RELAY_ON);
      }
    } else {
      Serial.println("Invalid valve index");
    }
  }

  // Handle scheduling messages
  else if (topicStr.startsWith("user/" + userBrokerId + "/" + controllerBrokerId + "/schedule/valve/")) {
    int valveIndex = topicStr.substring(topicStr.lastIndexOf("/") + 1).toInt();
    if (valveIndex >= 0 && valveIndex < numValves) {
      int delimiterPos1 = receivedPayload.indexOf('|');
      String startTime = receivedPayload.substring(delimiterPos1 + 1, receivedPayload.lastIndexOf('|'));
      String endTime = receivedPayload.substring(receivedPayload.lastIndexOf('|') + 1, receivedPayload.lastIndexOf('|', receivedPayload.lastIndexOf('|') - 1));
      String repetitionDaysStr = receivedPayload.substring(receivedPayload.lastIndexOf('|') + 1);
      uint8_t repetitionDayMask = 0;
      for (char c : repetitionDaysStr) {
        repetitionDayMask |= (1 << (c - '0' - 1));
      }
      Serial.print("Scheduling valve ");
      Serial.print(valveIndex);
      Serial.print(" from ");
      Serial.print(startTime);
      Serial.print(" to ");
      Serial.print(endTime);
      Serial.print(" on days ");
      Serial.println(repetitionDaysStr);
      scheduleValve(valveIndex, startTime, endTime, repetitionDayMask);
    } else {
      Serial.println("Invalid valve index");
    }
  }
}

void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  String message = (char*)payload;
  if (type == WStype_TEXT) {
    if (message.startsWith("valve:")) {
      String valveIndex = message.substring(6, message.indexOf(":"));
      String action = message.substring(message.indexOf(":") + 1);
      int valveNum = valveIndex.toInt();
      if (valveNum >= 0 && valveNum < numValves) {
        if (action == "open") {
          digitalWrite(motorPin, RELAY_ON);
          digitalWrite(waterValvePins[valveNum], RELAY_ON);
        } else if (action == "close") {
          digitalWrite(motorPin, !RELAY_ON);
          digitalWrite(waterValvePins[valveNum], !RELAY_ON);
        }
      }
    } else if (message.startsWith("schedule:")) {
      String scheduleInfo = message.substring(9);
      int delimiterPos = scheduleInfo.indexOf("|");
      String valveIndex = scheduleInfo.substring(0, delimiterPos);
      String startTime = scheduleInfo.substring(delimiterPos + 1, scheduleInfo.lastIndexOf("|"));
      String endTime = scheduleInfo.substring(scheduleInfo.lastIndexOf("|") + 1);
      String repetitionDaysStr = scheduleInfo.substring(scheduleInfo.lastIndexOf("|") + 7);
      uint8_t repetitionDayMask = repetitionDaysStr.toInt();
      int valveNum = valveIndex.toInt();
      if (valveNum >= 0 && valveNum < numValves) {
        scheduleValve(valveNum, startTime, endTime, repetitionDayMask);
      }
    }
  }
}

void setup() {
  Serial.begin(115200);

  for (int i = 0; i < numValves; i++) {
    pinMode(waterValvePins[i], OUTPUT);
    digitalWrite(waterValvePins[i], !RELAY_ON);
  }
  pinMode(motorPin, OUTPUT);
  digitalWrite(motorPin, !RELAY_ON);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  espClient.setCACert(ca_cert);
  mqtt_client.setKeepAlive(120);
  mqtt_client.setServer(mqtt_server, mqtt_port);

  if (!rtc.begin()) {
    Serial.println("Could not find RTC!");
  }
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  prefs.begin("irrigation_system", false);
  loadSchedulesFromPrefs();

  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);
  mqtt_client.setCallback(handleMqttMessage);
}

void loop() {
  if (!mqtt_client.connected()) {
    reconnect();
  }
  mqtt_client.loop();
  webSocket.loop();
  checkScheduledIrrigations();
}

void reconnect() {
  while (!mqtt_client.connected()) {
    Serial.print("Attempting MQTT connection... ");
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

void loadSchedulesFromPrefs(){
  for (int i = 0; i < numValves; i++) {
    String scheduleKey = "valve_" + String(i) + "_schedule";
    if (prefs.getString(scheduleKey.c_str(), "").length() > 0) {
      String scheduleData = prefs.getString(scheduleKey.c_str(), "");
      int delimiterPos1 = scheduleData.indexOf("|");
      String isActiveStr = scheduleData.substring(0, delimiterPos1);
      int delimiterPos2 = scheduleData.indexOf("|", delimiterPos1 + 1);
      String startTimeStr = scheduleData.substring(delimiterPos1 + 1, delimiterPos2);
      String endTimeStr = scheduleData.substring(delimiterPos2 + 1);
      String repetitionDaysStr = scheduleData.substring(delimiterPos2 + 1);
      valveSchedules[i].isActive = (isActiveStr == "true");
      valveSchedules[i].startTime = DateTime(rtc.now().unixtime() + parseTimeString(startTimeStr));
      valveSchedules[i].endTime = DateTime(rtc.now().unixtime() + parseTimeString(endTimeStr));
      uint8_t repetitionDayMask = 0;
      for (char c : repetitionDaysStr) {
        repetitionDayMask |= (1 << (c - '0' - 1));
      }
      valveSchedules[i].repetitionDays = repetitionDayMask;
    } else {
      valveSchedules[i].isActive = false;
    }
  }
}

void saveSchedulesToPrefs() {
  for (int i = 0; i < numValves; i++) {
    String scheduleKey = "valve_" + String(i) + "_schedule";
    String scheduleData = "";
    String letter = valveSchedules[i].isActive ? "true" : "false";
    scheduleData += letter + "|";
    scheduleData += String(valveSchedules[i].startTime.hour()) + ":" + String(valveSchedules[i].startTime.minute()) + "|";
    scheduleData += String(valveSchedules[i].endTime.hour()) + ":" + String(valveSchedules[i].endTime.minute()) + "|";
    String repetitionDaysStr = "";
    for (int j = 0; j < 7; j++) {
      if ((valveSchedules[i].repetitionDays & (1 << j)) > 0) {
        repetitionDaysStr += String(j + 1);
      }
    }
    scheduleData += repetitionDaysStr;
    prefs.putString(scheduleKey.c_str(), scheduleData);
  }
}

