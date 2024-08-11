/**
     * @file main.cpp
     * @brief This code is for an ESP32 device that controls various components of a greenhouse or indoor gardening system.
     * It includes functionality for controlling lights, fans, water valves, and ventilation roller shutters.
     * It also reads and displays environmental data from a BME680 sensor, and communicates with an MQTT broker
     * and a WebSocket server for remote monitoring and control.
     * @author Pem Tshewang
     * @date 7 march  2024
**/

    #include <WiFi.h>

    #include <WebSocketsServer.h>

    #include <PubSubClient.h>

    #include <Wire.h>

    #include "RTClib.h"

    #include <Preferences.h>

    #include <Adafruit_Sensor.h>

    #include <Adafruit_BME680.h>

    #include <Adafruit_GFX.h>

    #include <Adafruit_SSD1306.h>

    #include <WiFiClientSecure.h>

    /**
     * @brief Width of the OLED display in pixels.
     */
    #define SCREEN_WIDTH 128

    /**
     * @brief Height of the OLED display in pixels.
     */
    #define SCREEN_HEIGHT 64

    /**
     * @brief Reset pin for the OLED display (-1 means no reset pin).
     */
    #define OLED_RESET -1

    /**
     * @brief I2C address of the OLED display.
     */
    #define SCREEN_ADDRESS 0x3C

    /**
     * @brief Logic level for turning on a relay (LOW in this case).
     */
    #define RELAY_ON LOW

    Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, & Wire, OLED_RESET);
    RTC_DS3231 rtc;
    WiFiClientSecure espClient;
    PubSubClient mqtt_client(espClient);
    WebSocketsServer webSocket = WebSocketsServer(80);
    Preferences prefs;
    Adafruit_BME680 bme;

    /**
     * @brief URL or IP address of the MQTT broker.
     */
    const char *mqtt_server = "meb1e91e.ala.asia-southeast1.emqxsl.com";

    /**
     * @brief Username for authenticating with the MQTT broker.
     */
    const char *mqtt_username = "Deki wangmo";

    /**
     * @brief Password for authenticating with the MQTT broker.
     */
    String mqtt_password = "$2b$10$Gi1vRWSSfQXwON7pNzVAy.b3.uxc4PEoK0FitMLGV4iGjgGvK2gZa";

    /**
     * @brief Port number for the MQTT broker (default is 8883).
     */
    const int mqtt_port = 8883;

    /**
     * @brief Broker ID for the controller.
     */
    static String controllerBrokerId = "85a2afbd-d";

    /**
     * @brief Broker ID for the user.
     */
    static String userBrokerId = "DE38472920";

    /**
     * @brief Keeps track of the last time data was published to the MQTT broker.
     */
    unsigned long lastMqttPublishTime = 0;

    // EMQX serverless deployment certificate
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
    /**
     * @brief Pin number for controlling a light.
     */
    const int lightPin = 2;

    /**
     * @brief Pin number for controlling an exhaust fan.
     */
    const int exFanPin = 4;

    /**
     * @brief Pin number for reading soil moisture data.
     */
    const int soilMoisturePin = 36;

    /**
     * @brief Pin number for controlling a water valve.
     */
    const int waterValvePin = 13;

    /**
     * @brief Pin number for controlling the up movement of the right ventilation roller shutter.
     */
    const int rightVentilationRollerShutterPinUp = 12;

    /**
     * @brief Pin number for controlling the down movement of the right ventilation roller shutter.
     */
    const int rightVentilationRollerShutterPinDown = 14;

    /**
     * @brief Pin number for controlling the up movement of the left ventilation roller shutter.
     */
    const int leftVentilationRollerShutterPinUp = 27;

    /**
     * @brief Pin number for controlling the down movement of the left ventilation roller shutter.
     */
    const int leftVentilationRollerShutterPinDown = 26;

    const int motorPin = 25;

    /**
     * @brief Wi-Fi SSID for connecting to the local network.
     */
    const char * ssid = "welovewifi";

    /**
     * @brief Wi-Fi password for connecting to the local network.
     */
    const char * password = "noteasypassword";

    /**
     * @brief Analog value representing dry soil.
     */
    const int dryValue = 850;

    /**
     * @brief Analog value representing wet soil.
     */
    const int wetValue = 200;

    const int ldrPin = 34;
    /**
     * @brief Flag indicating whether the exhaust fan is manually turned on or off.
     */
    bool isFanManuallyOn = false;
    
    bool isLightManuallyOn = false;

    /**
     * @brief Flag indicating whether the water valve is manually opened or closed.
     */
    bool isWaterValveManuallyOn = false;

    /**
     * @brief Flag indicating whether the water valve is scheduled to open or close.
     */
    bool isWaterValveScheduled = false;

    /**
     * @brief Flag indicating whether the right ventilation roller shutter is manually controlled or not.
     */
    bool rightRollerShutterManuallyOn = false;

    /**
     * @brief Flag indicating whether the left ventilation roller shutter is manually controlled or not.
     */
    bool leftRollerShutterManuallyOn = false;

    /**
     * @brief Converts an analog sensor value to a percentage value representing soil moisture.
     *
     * @param sensorValue The analog sensor value to be converted.
     * @return The soil moisture percentage (0-100).
     */
    int getMoisturePercentage(int sensorValue) {
      return map(sensorValue, dryValue, wetValue, 0, 100);
    }

    /**
     * @brief Clears a specific slot from the Preferences storage.
     *
     * @param slotNumber The number of the slot to be cleared.
     */
    void clearSlot(int slotNumber) {
      String slotKey = "slot" + String(slotNumber);
      prefs.begin(slotKey.c_str(), false);
      Serial.println("Slot " + String(slotNumber) + " cleared");
      prefs.clear();
      prefs.end();
    }

    /**
     * @brief Checks a specific slot for scheduled watering events.
     *
     * @param slotNumber The number of the slot to be checked.
     */
    void checkSlot(int slotNumber) {
      String slotKey = "slot" + String(slotNumber);
      prefs.begin(slotKey.c_str(), false);
      int repetitionDays = prefs.getInt("repetitionDays");
      String startTime = prefs.getString("start");
      String endTime = prefs.getString("end");
      prefs.end();

      DateTime now = rtc.now();
      int currentDay = now.dayOfTheWeek();
      char currentTimeStr[9];
      sprintf(currentTimeStr, "%02d:%02d", now.hour(), now.minute());

      bool isCurrentDayIncluded = repetitionDays & (1 << currentDay);
      if (isCurrentDayIncluded) {
        Serial.println("Current Included");
        if (startTime == String(currentTimeStr) && !isWaterValveScheduled) {
          // Perform actions for this slot
          Serial.println("Slot " + String(slotNumber) + " - Water valve is open");
          digitalWrite(motorPin, RELAY_ON);
          digitalWrite(waterValvePin, RELAY_ON);
          isWaterValveScheduled = true;
          // Additional actions for this slot if needed
        }
        if (String(currentTimeStr) == endTime && isWaterValveScheduled) {
          // Additional actions or cleanup for this slot
          Serial.println("Slot " + String(slotNumber) + " - Water valve is closed");
          digitalWrite(motorPin, !RELAY_ON);
          digitalWrite(waterValvePin, !RELAY_ON);
          isWaterValveScheduled = false;
        }
      }
    }

    /**
     * @brief Stores scheduled watering dates in the Preferences storage.
     * @param slotNumber The number of the slot to store the schedule. @param startTime The start time for the watering schedule.
     * @param endTime The end time for the watering schedule.
     * @param repetitionDays A bitmask representing the days of the week for the schedule.
     */
    void storeScheduledDates(int slotNumber, String startTime, String endTime, int repetitionDays) {
      switch (slotNumber) {
      case 1:
        Serial.println("Slot 1 storing");
        prefs.begin("slot1", false);
        prefs.putString("start", startTime);
        prefs.putString("end", endTime);
        prefs.putInt("repetitionDays", repetitionDays);
        prefs.end();
        break;
      case 2:
        Serial.println("Slot 2 storing");
        prefs.begin("slot2", false);
        prefs.putString("start", startTime);
        prefs.putString("end", endTime);
        prefs.putInt("repetitionDays", repetitionDays);
        prefs.end();
        break;
      case 3:
        Serial.println("Slot 3 storing");
        prefs.begin("slot3", false);
        prefs.putString("start", startTime);
        prefs.putString("end", endTime);
        prefs.putInt("repetitionDays", repetitionDays);
        prefs.end();
        break;
      }
    }

    unsigned long lastReadingTime = 0;

    /**
     * @brief Handles MQTT messages for scheduling watering events.
     *
     * @param slotNumber The number of the slot for the watering schedule.
     * @param message The message containing the schedule information.
     */
    void handleMqttWSchedule(const String & slotNumber,
      const String & message) {
      Serial.println(message);
      int firstDelimiterPos = message.indexOf('|');
      int secondDelimiterPos = message.indexOf('|', firstDelimiterPos + 1);

      const String startTime = message.substring(0, firstDelimiterPos);
      const String endTime = message.substring(firstDelimiterPos + 1, secondDelimiterPos);
      const String repetitionDaysStr = message.substring(secondDelimiterPos + 1);

      int repetitionDays = repetitionDaysStr.toInt(); // Convert repetitionDaysStr to an integer

      Serial.println("Slot: " + slotNumber);
      Serial.println("Start Time: " + startTime);
      Serial.println("End Time: " + endTime);
      Serial.println(repetitionDays);
      // convert slotNumber to int
      storeScheduledDates(slotNumber.toInt(), startTime, endTime, repetitionDays);
    }
    /**
     * @brief Handles MQTT messages for setting environmental thresholds.
     *
     * @param thresholdType The type of threshold (temperature, humidity, or soilMoisture).
     * @param thresholdValue The value of the threshold.
     */
    void handleMqttThresholdSetter(const String & thresholdType,
      const String & thresholdValue) {
      prefs.begin("my-app", false);
      if (thresholdType == "temperature") {
        prefs.putFloat("temp", thresholdValue.toFloat());
      } else if (thresholdType == "humidity") {
        prefs.putFloat("hum", thresholdValue.toFloat());
      } else if (thresholdType == "soilMoisture") {
        prefs.putFloat("soil", thresholdValue.toFloat());
      }
      prefs.end();
    }

    /**
     * @brief Handles device control messages received via MQTT or WebSocket.
     *
     * @param topic The topic or device name for the control message.
     * @param message The control message (e.g., "on", "off", "open", "close").
     */
    void handleDeviceControl(const String & topic,
      const String & message) {
      Serial.println("The topic is: ");
      Serial.println(topic);
      if (topic == "light") {
      if(message=="on"){
        isLightManuallyOn = true;
        digitalWrite(lightPin,RELAY_ON);
      }else{
        isLightManuallyOn = false;
        digitalWrite(lightPin,!RELAY_ON);
      }
        
      } else if (topic == "ventilationFan") {
        Serial.println("Ventilation fan is " + message);
        isFanManuallyOn = message == "on";
        digitalWrite(exFanPin, (message == "on") ? !RELAY_ON : RELAY_ON);
      } else if (topic == "waterValve") {
        Serial.println("Water valve is " + message);
        isWaterValveManuallyOn = message == "open";
        if (message == "open") {
          digitalWrite(motorPin, RELAY_ON);
          digitalWrite(waterValvePin, RELAY_ON);
          Serial.println("Water valve is open");
        } else {
          digitalWrite(motorPin, !RELAY_ON);
          digitalWrite(waterValvePin, !RELAY_ON);
          Serial.println("Water valve is closed");
        }
      } else if (topic == "schedule") {
        int firstDelimiterPos = message.indexOf('|');
        int secondDelimiterPos = message.indexOf('|', firstDelimiterPos + 1);
        int thirdDelimiterPos = message.indexOf('|', secondDelimiterPos + 1);

        String slotNumber = message.substring(0, firstDelimiterPos);
        String startTime = message.substring(firstDelimiterPos + 1, secondDelimiterPos);
        String endTime = message.substring(secondDelimiterPos + 1, thirdDelimiterPos);
        String repetitionDaysStr = message.substring(thirdDelimiterPos + 1);

        // Extracting values for the last part (repetitionDays)
        int lastDelimiterPos = repetitionDaysStr.indexOf('|');
        if (lastDelimiterPos != -1) {
          repetitionDaysStr = repetitionDaysStr.substring(0, lastDelimiterPos);
        }
        int repetitionDays = repetitionDaysStr.toInt(); // Convert repetitionDaysStr to an integer

        Serial.println("Slot: " + slotNumber);
        Serial.println("Start Time: " + startTime);
        Serial.println("End Time: " + endTime);
        Serial.println("Repetition Days: " + repetitionDays);
        // convert slotNumber to int
        storeScheduledDates(slotNumber.toInt(), startTime, endTime, repetitionDays);
      } else if (topic == "scheduleClear") {
        clearSlot(message.toInt());
      } else if (topic == "rollerShutterLeft") {
        Serial.println("Left roller shutter is " + message);
        if (message == "up") {
          Serial.println("Left roller shutter up");
          leftRollerShutterManuallyOn = true;
          digitalWrite(leftVentilationRollerShutterPinUp, RELAY_ON);
          digitalWrite(leftVentilationRollerShutterPinDown, !RELAY_ON);
        } else if (message == "down") {
          Serial.println("Left roller shutter down");
          leftRollerShutterManuallyOn = false;
          digitalWrite(leftVentilationRollerShutterPinUp, !RELAY_ON);
          digitalWrite(leftVentilationRollerShutterPinDown, RELAY_ON);
        }
      } else if (topic == "rollerShutterRight") {
        if (message == "up") {
          rightRollerShutterManuallyOn = true;
          Serial.println("Right roller shutter up");
          digitalWrite(rightVentilationRollerShutterPinUp, RELAY_ON);
          digitalWrite(rightVentilationRollerShutterPinDown, !RELAY_ON);
        } else if (message == "down") {
          rightRollerShutterManuallyOn = false;
          Serial.println("Right roller shutter down");
          digitalWrite(rightVentilationRollerShutterPinUp, !RELAY_ON);
          digitalWrite(rightVentilationRollerShutterPinDown, RELAY_ON);
        }
      }
    }

    /**
     * @brief Extracts the main topic index from a given string.
     *
     * @param str The input string.
     * @return The index of the third slash ("/") in the string, or -1 if not found.
     */
    int extractMainTopicIndex(String str) {
      int firstSlashPos = str.indexOf("/"); // Find the position of the first "/"
      if (firstSlashPos != -1) {
        int secondSlashPos = str.indexOf("/", firstSlashPos + 1); // Find the position of the second "/"
        if (secondSlashPos != -1) {
          int thirdPos = str.indexOf("/", secondSlashPos + 1); // Find the position of the third "/"
          return thirdPos;
        }
      }
      return -1;
    }

    /**
     * @brief Callback function for handling MQTT messages.
     *
     * @param topic The topic of the received message.
     * @param payload The payload of the received message.
     * @param length The length of the received message.
     */
    void callback(char * topic, byte * payload, unsigned int length) {
      String receivedTopic = String(topic);
      String receivedPayload;
      for (int i = 0; i < length; i++) {
        receivedPayload += (char) payload[i];
      }
      int lastSlashIndex = extractMainTopicIndex(receivedTopic);
      const String modifiedTopic = receivedTopic.substring(lastSlashIndex + 1);
      Serial.println("Received mtopic: " + modifiedTopic);
      const int firstSlashPosition = modifiedTopic.indexOf("/");
      const String identifier = modifiedTopic.substring(0, firstSlashPosition);
      Serial.println("Received identifier: " + identifier);
      const String extractedTopic = modifiedTopic.substring(firstSlashPosition + 1);
      if (identifier == "actuator") {
        handleDeviceControl(extractedTopic, receivedPayload);
      } else if (identifier == "wschedule") {
        handleMqttWSchedule(extractedTopic, receivedPayload);
      } else if (identifier == "threshold") {
        handleMqttThresholdSetter(extractedTopic, receivedPayload);
      }
    }

    /**
     * @brief Callback function for handling WebSocket events.
     *
     * @param num The client ID of the WebSocket connection.
     * @param type The type of WebSocket event.
     * @param payload The payload of the WebSocket event.
     * @param length The length of the payload.
     */
    void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
      if (type == WStype_CONNECTED) {
        Serial.println("Connected to WebSocket");
      } else if (type == WStype_TEXT) {
        if (length > 0) {
          String message = (char * ) payload;
          if (message == "ping") {
            // Respond to 'ping' with 'pong'
            webSocket.sendTXT(num, "pong");
          } else if (message.startsWith("threshold:")) {
            // Update the threshold value based on the received message
            String thresholdType = message.substring(message.indexOf(':') + 1, message.lastIndexOf(':'));
            float thresholdValue = message.substring(message.lastIndexOf(':') + 1).toFloat();
            prefs.begin("my-app", false);
            if (thresholdType == "temperature") {
              prefs.putFloat("temp", thresholdValue);
            } else if (thresholdType == "humidity") {
              prefs.putFloat("hum", thresholdValue);
            } else if (thresholdType == "soilMoisture") {
              prefs.putFloat("soil", thresholdValue);
            }
            prefs.end();
          } else if (message.startsWith("scheduleClear|")) {
            // Clear a scheduled watering slot
            String slotNumber = message.substring(14); // Skip "scheduleClear|"
            clearSlot(slotNumber.toInt());
          } else if (message.startsWith("schedule|")) {
            // Store a new watering schedule
            String scheduleInfo = message.substring(9); // Skip "schedule|"
            int firstDelimiterPos = scheduleInfo.indexOf('|');
            int secondDelimiterPos = scheduleInfo.indexOf('|', firstDelimiterPos + 1);
            int thirdDelimiterPos = scheduleInfo.indexOf('|', secondDelimiterPos + 1);

            String slotNumber = scheduleInfo.substring(0, firstDelimiterPos);
            String startTime = scheduleInfo.substring(firstDelimiterPos + 1, secondDelimiterPos);
            String endTime = scheduleInfo.substring(secondDelimiterPos + 1, thirdDelimiterPos);
            String repetitionDaysStr = scheduleInfo.substring(thirdDelimiterPos + 1);

            int repetitionDays = repetitionDaysStr.toInt(); // Convert repetitionDaysStr to an integer

            Serial.println("Slot: " + slotNumber);
            Serial.println("Start Time: " + startTime);
            Serial.println("End Time: " + endTime);
            Serial.println("Repetition Days: " + repetitionDays);
            // convert slotNumber to int
            storeScheduledDates(slotNumber.toInt(), startTime, endTime, repetitionDays);
          } else {
            // Handle device control messages
            String device = message.substring(0, message.indexOf(':'));
            String action = message.substring(message.indexOf(':') + 1);
            String topic = device;
            handleDeviceControl(topic, action);
          }
        }
      }
    }

    /**
     * @brief Reconnects to the MQTT broker if the connection is lost.
     */
    void reconnect() {
      // Loop until we're reconnected
      while (!mqtt_client.connected()) {
        Serial.print("Attempting MQTT connection… ");
        String clientId = "ESP32Client";
        // Attempt to connect
        if (mqtt_client.connect(clientId.c_str(), mqtt_username, mqtt_password.c_str())) {
          display.println();
          display.println("Connected to MQTT");
          display.display();
          Serial.println("connected!");
          // subscribe to every topic
          // topic format: user/userBrokerId/topics
          mqtt_client.subscribe(("user/"+userBrokerId+"/#").c_str());
          Serial.println("user/"+userBrokerId+"/#");
        } else {
          Serial.print("failed, rc = ");
          Serial.print(mqtt_client.state());
          Serial.println(" try again in 5 seconds");
          // Wait 5 seconds before retrying
          delay(5000);
        }
      }
    }

    /**
     * @brief Initial setup function.
     */
    void setup() {
      Serial.begin(115200);
      pinMode(lightPin, OUTPUT);
      pinMode(exFanPin, OUTPUT);
      pinMode(waterValvePin, OUTPUT);
      pinMode(rightVentilationRollerShutterPinUp, OUTPUT);
      pinMode(rightVentilationRollerShutterPinDown, OUTPUT);
      pinMode(leftVentilationRollerShutterPinUp, OUTPUT);
      pinMode(leftVentilationRollerShutterPinDown, OUTPUT);
      pinMode(soilMoisturePin, INPUT);
      pinMode(motorPin, OUTPUT);
      pinMode(ldrPin,INPUT);
      digitalWrite(lightPin, !RELAY_ON);
      digitalWrite(exFanPin, !RELAY_ON);
      digitalWrite(waterValvePin, !RELAY_ON);
      digitalWrite(rightVentilationRollerShutterPinUp, !RELAY_ON);
      digitalWrite(rightVentilationRollerShutterPinDown, !RELAY_ON);
      digitalWrite(leftVentilationRollerShutterPinUp, !RELAY_ON);
      digitalWrite(leftVentilationRollerShutterPinDown, !RELAY_ON);
      digitalWrite(motorPin, !RELAY_ON);

      if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;)
        ;
      }
      display.display(); // Initialize with an empty display

      delay(2000); // Pause for 2 seconds
      display.clearDisplay();

      if (!rtc.begin()) {
        Serial.println("Could not find RTC! Check circuit.");
        while (1)
        ;
      }
      // first time initialize to get time
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
      
      if (rtc.lostPower()) {
        Serial.println("RTC lost power, lets set the time!");
        // following line sets the RTC to the date & time this sketch was compiled
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
      }

      if (!bme.begin()) {
        Serial.println("Could not find a valid BME680 sensor, check wiring!");
        while (1)
        ;
      }

      prefs.begin("my-app", false);
      static float defaultThreshold = 25.0;
      prefs.putFloat("temp", defaultThreshold);
      randomSeed(micros());
      
      WiFi.mode(WIFI_STA);
      WiFi.softAP("esp32", "12345678");
      
      WiFi.begin(ssid, password);
      while (WiFi.status() != WL_CONNECTED) {
        Serial.println("Connecting to wifi.....");
        delay(1000);
      }
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.println(F("localIP:"));
      display.print(WiFi.localIP());
      display.display();

      webSocket.begin();
      webSocket.onEvent(onWebSocketEvent);
      // only for mqtts
      espClient.setCACert(ca_cert);
      mqtt_client.setKeepAlive(120);
      mqtt_client.setServer(mqtt_server, mqtt_port);
      mqtt_client.setCallback(callback);
      prefs.end();
      delay(500);
    }

    /**
     * @brief Main loop function.
     */
    
    void printTime(DateTime now) {
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        // Clear the line by printing spaces
        display.setCursor(0, 40);
        display.print("                           u");
        // Move the cursor back to the start of the line and print the updated time
        display.setCursor(0, 40);
        char timeStr[16];
        sprintf(timeStr, "%02d:%02d", now.hour(), now.minute());
        display.print("Time: ");
        display.println(timeStr);
        display.display();
    }
     
    static unsigned long lastTimeUpdate = 0;
    const unsigned long timeUpdateInterval = 1000; // Update every second
    void loop() {
      // printing the time on rtc
      DateTime now = rtc.now();
      if (millis() - lastTimeUpdate >= timeUpdateInterval) {
        lastTimeUpdate = millis();
        printTime(now);
      }
      
      int ldrValue = digitalRead(ldrPin);
      
      if (ldrValue == 1) {
        // off logic
        if(!isLightManuallyOn){
          digitalWrite(lightPin, RELAY_ON); 
        }
      } else {
        // on logic
        digitalWrite(lightPin, !RELAY_ON); 
      }
      
      if (!mqtt_client.connected())
        reconnect();
      mqtt_client.loop();
      webSocket.loop();

      prefs.begin("my-app", false);
      float tempThreshold = prefs.getFloat("temp");
      float humThreshold = prefs.getFloat("hum");
      prefs.end();

      Serial.println(tempThreshold);
      Serial.println(humThreshold);

      for (int i = 1; i <= 3; i++) {
        checkSlot(i);
        // You can add delays or other handling if needed between checking slots
        delay(100); // Example delay between slot checks
      }

      Serial.println("------");

      float temperature = bme.readTemperature();
      float humidity = bme.readHumidity();
      Serial.println(temperature);
      Serial.println(humidity);

      if (temperature > tempThreshold && !isnan(temperature)) {
        Serial.println("Temperature is greater than threshold");
        digitalWrite(exFanPin, RELAY_ON);
        digitalWrite(rightVentilationRollerShutterPinUp, RELAY_ON);
        digitalWrite(leftVentilationRollerShutterPinUp, RELAY_ON);
        digitalWrite(rightVentilationRollerShutterPinDown, !RELAY_ON);
        digitalWrite(leftVentilationRollerShutterPinDown, !RELAY_ON);
      }

      if (humidity > humThreshold && !isnan(humidity)) {
        Serial.println("Humidity is greater than threshold");
        digitalWrite(exFanPin, RELAY_ON);
        digitalWrite(rightVentilationRollerShutterPinUp, RELAY_ON);
        digitalWrite(leftVentilationRollerShutterPinUp, RELAY_ON);
        digitalWrite(rightVentilationRollerShutterPinDown, !RELAY_ON);
        digitalWrite(leftVentilationRollerShutterPinDown, !RELAY_ON);
      }

      if (temperature < tempThreshold || humidity < humThreshold) {
        if (!isFanManuallyOn) {
          digitalWrite(exFanPin, !RELAY_ON);
        }
        if (!leftRollerShutterManuallyOn) {
          digitalWrite(leftVentilationRollerShutterPinUp, !RELAY_ON);
          digitalWrite(leftVentilationRollerShutterPinDown, RELAY_ON);
        }
        if (!rightRollerShutterManuallyOn) {
          digitalWrite(rightVentilationRollerShutterPinUp, !RELAY_ON);
          digitalWrite(rightVentilationRollerShutterPinDown, RELAY_ON);
        }
      }

      unsigned long currentTime = millis();
      if (currentTime - lastReadingTime >= 1 * 60 * 1000 && webSocket.connectedClients() > 0) {
        // the readings are sent after 30 seconds from the current elapsed time
        int soilMoisture = getMoisturePercentage(analogRead(soilMoisturePin));
        Serial.println("SoilMoisture:");
        Serial.println(soilMoisture);
        if (!isnan(temperature) && !isnan(humidity && !isnan(soilMoisture))) {
          webSocket.broadcastTXT("temperature:" + String(temperature));
          webSocket.broadcastTXT("humidity:" + String(humidity));
          webSocket.broadcastTXT("soilMoisture:" + String(soilMoisture));
          webSocket.broadcastTXT("light:" + String(soilMoisture));
        } else {
          Serial.println("Failed to read from BME sensor!");
        }
        lastReadingTime = currentTime;
      }

      static unsigned long lastMqttPublishTime = 0;
      if (currentTime - lastMqttPublishTime >= 1 * 60 * 1000) {
        Serial.println("Next message will be published after 7 minutes");
        int soilMoisture = getMoisturePercentage(analogRead(soilMoisturePin));
        if (!isnan(temperature) && !isnan(humidity) && !isnan(soilMoisture)) {
          String mqttMessage = "temperature:" + String(temperature) + "|humidity:" + String(humidity) + "|soilMoisture:" + String(soilMoisture) +"|"+"light:"+String(ldrValue);
          mqtt_client.publish(("user/" + userBrokerId + "/" + controllerBrokerId + "/readings").c_str(), mqttMessage.c_str());
          lastMqttPublishTime = currentTime;
        } else {
          Serial.println("Failed to read from BME sensor!");
        }
    }
  } 
