#include <ESP8266WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>

const char* ssid = "JoeMama";
const char* password = "2A0R0M4AAN";

String masterIP = "";
int masterPort = 81;
bool masterDiscovered = false;

WiFiUDP udp;
const int UDP_PORT = 4210;
const char* DISCOVERY_MESSAGE = "SMART_PARKING";

const unsigned long DISCOVERY_TIMEOUT = 60000;
unsigned long discoveryStartTime = 0;

String deviceMAC = "";
int deviceID = -1;

const int TRIG_PIN = 5;  // Trigger pin connected to D1
const int ECHO_PIN = 4;  // Echo pin connected to D2

const int RED_LED_PIN = 2;    // D4
const int GREEN_LED_PIN = 0;  // D3

WebSocketsClient webSocket;

long duration;
int distance;
unsigned long lastReadingTime = 0;
const unsigned long READ_INTERVAL = 2000;

bool listenForDiscovery(unsigned long timeout) {
  unsigned long startTime = millis();
  Serial.println("Listening for master discovery broadcast...");
  
  disconnectionStartTime = 0;
  wasConnected = false;
  
  digitalWrite(RED_LED_PIN, HIGH);
  digitalWrite(GREEN_LED_PIN, HIGH);
  
  udp.begin(UDP_PORT);
  
  while (millis() - startTime < timeout) {
    int packetSize = udp.parsePacket();
    if (packetSize) {
      Serial.printf("Received UDP packet of size %d from %s:%d\n", 
                   packetSize, udp.remoteIP().toString().c_str(), udp.remotePort());
      
      char packetBuffer[256];
      int len = udp.read(packetBuffer, 255);
      if (len > 0) {
        packetBuffer[len] = 0;
      }
      
      Serial.printf("Packet contents: %s\n", packetBuffer);
      
      DynamicJsonDocument doc(256);
      DeserializationError error = deserializeJson(doc, packetBuffer);
      
      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        continue;
      }
      
      const char* type = doc["type"];
      const char* message = doc["message"];
      
      if (type && message && strcmp(type, "discovery") == 0 && 
          strcmp(message, DISCOVERY_MESSAGE) == 0) {
        
        const char* ip = doc["ip"];
        int port = doc["port"];
        
        if (ip) {
          masterIP = String(ip);
          if (port > 0) {
            masterPort = port;
          }
          
          Serial.printf("Master discovered at %s:%d\n", masterIP.c_str(), masterPort);

          for (int i = 0; i < 3; i++) {
            digitalWrite(GREEN_LED_PIN, HIGH);
            delay(100);
            digitalWrite(GREEN_LED_PIN, LOW);
            delay(100);
          }
          
          return true;
        }
      }
    }
    
    if (millis() % 500 < 250) {
      digitalWrite(RED_LED_PIN, HIGH);
      digitalWrite(GREEN_LED_PIN, LOW);
    } else {
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(GREEN_LED_PIN, HIGH);
    }
    
    delay(10);
  }
  
  Serial.println("Discovery timeout!");
  
  for (int i = 0; i < 5; i++) {
    digitalWrite(RED_LED_PIN, HIGH);
    delay(100);
    digitalWrite(RED_LED_PIN, LOW);
    delay(100);
  }
  
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, LOW);
  
  return false;
}

void connectToWebSocket() {
  if (masterIP.length() > 0) {
    Serial.printf("Connecting to WebSocket server at %s:%d\n", masterIP.c_str(), masterPort);
    webSocket.begin(masterIP.c_str(), masterPort, "/");
    webSocket.onEvent(webSocketEvent);
    webSocket.setReconnectInterval(5000);
    masterDiscovered = true;
  } else {
    Serial.println("Cannot connect: Master IP unknown");
    masterDiscovered = false;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("ESP8266 Slave - Smart Parking System");

  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, LOW);
  
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  deviceMAC = WiFi.macAddress();
  Serial.println("Device MAC: " + deviceMAC);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  discoveryStartTime = millis();
  if (listenForDiscovery(DISCOVERY_TIMEOUT)) {
    connectToWebSocket();
  } else {
    Serial.println("No master found, will keep trying");
  }
  
  Serial.println("Setup complete");
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("Disconnected from WebSocket server");
      
      // Only modify LEDs if there's no sensor error
      if (!sensorError) {
        digitalWrite(RED_LED_PIN, LOW);
        digitalWrite(GREEN_LED_PIN, LOW);
        
        digitalWrite(RED_LED_PIN, HIGH);
        delay(100);
        digitalWrite(RED_LED_PIN, LOW);
      }

      if (disconnectionStartTime == 0) {
        disconnectionStartTime = millis();
        Serial.println("Starting disconnection timer");
      }
      break;
      
    case WStype_CONNECTED:
      {
        Serial.println("Connected to WebSocket server");
        
        disconnectionStartTime = 0;

        // Only modify LEDs if there's no sensor error
        if (!sensorError) {
          digitalWrite(GREEN_LED_PIN, HIGH);
          delay(100);
          digitalWrite(GREEN_LED_PIN, LOW);
          delay(100);
          digitalWrite(GREEN_LED_PIN, HIGH);
          delay(100);
          digitalWrite(GREEN_LED_PIN, LOW);
        }
        
        DynamicJsonDocument regDoc(256);
        regDoc["type"] = "register";
        regDoc["mac"] = deviceMAC;
        regDoc["currentId"] = deviceID;
        
        String regJson;
        serializeJson(regDoc, regJson);
        webSocket.sendTXT(regJson);
        Serial.println("Sent registration request with MAC: " + deviceMAC);
      }
      break;
      
    case WStype_TEXT:
      {
        Serial.printf("Received text: %s\n", payload);
        
        DynamicJsonDocument doc(256);
        DeserializationError error = deserializeJson(doc, payload);
        
        if (error) {
          Serial.print("deserializeJson() failed: ");
          Serial.println(error.c_str());
          return;
        }
        
        if (doc.containsKey("type") && strcmp(doc["type"], "id_assigned") == 0) {
          int newId = doc["assigned_id"];
          deviceID = newId;
          Serial.printf("Master assigned ID: %d to this device\n", deviceID);
          
          sendDistanceReading();
          return;
        }
        
        if (doc.containsKey("id")) {
          int receivedId = doc["id"];
          if (receivedId == deviceID) {
            String ledStatus = doc["led"];
            
            // Only change LEDs if there's no sensor error
            if (!sensorError) {
              if (ledStatus == "red") {
                digitalWrite(RED_LED_PIN, HIGH);
                digitalWrite(GREEN_LED_PIN, LOW);
                Serial.println("Setting RED LED ON (spot occupied)");
              } else if (ledStatus == "green") {
                digitalWrite(RED_LED_PIN, LOW);
                digitalWrite(GREEN_LED_PIN, HIGH);
                Serial.println("Setting GREEN LED ON (spot available)");
              }
            } else {
              Serial.println("Ignoring LED command due to sensor error");
            }
          }
        }
      }
      break;
  }
}

// Add variables for sensor error detection
bool sensorError = false;
const int MAX_SENSOR_ERROR_COUNT = 3;
int sensorErrorCount = 0;
unsigned long lastSensorErrorFlash = 0;
const unsigned long ERROR_FLASH_INTERVAL = 300; // 300ms for error LED flashing

int readDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  // Use timeout for pulseIn to detect sensor errors
  duration = pulseIn(ECHO_PIN, HIGH, 23000); // 23ms timeout (for max range ~400cm)
  
  if (duration == 0) {
    // No echo received within timeout - sensor might be disconnected or faulty
    Serial.println("WARNING: No echo received from ultrasonic sensor - possible hardware error");
    sensorErrorCount++;
    
    if (sensorErrorCount >= MAX_SENSOR_ERROR_COUNT) {
      sensorError = true;
    }
    
    return -1; // Error value
  }
  
  // Valid reading received, reset error counter
  sensorErrorCount = 0;
  sensorError = false;
  
  int calculatedDistance = duration * 0.034 / 2;
  
  if (calculatedDistance > 200) {
    calculatedDistance = 200;
  }
  
  return calculatedDistance;
}

void sendDistanceReading() {
  if (webSocket.isConnected()) {
    if (deviceID < 0) {
      Serial.println("No valid device ID assigned yet. Cannot send reading.");
      return;
    }
    
    distance = readDistance();
    
    // Check for sensor error
    if (distance == -1) {
      Serial.println("ERROR: Failed to get valid distance reading");
      
      // Still send a message to the master, but with error flag
      DynamicJsonDocument doc(256);
      doc["id"] = deviceID;
      doc["mac"] = deviceMAC;
      doc["distance"] = -1;
      doc["error"] = "sensor_error";
      
      String jsonString;
      serializeJson(doc, jsonString);
      webSocket.sendTXT(jsonString);
      
      Serial.println("Sent sensor error notification to master");
    } else {
      // Normal reading, send as usual
      DynamicJsonDocument doc(256);
      doc["id"] = deviceID;
      doc["mac"] = deviceMAC;
      doc["distance"] = distance;
      
      String jsonString;
      serializeJson(doc, jsonString);
      webSocket.sendTXT(jsonString);
      
      Serial.print("Sent distance reading: ");
      Serial.print(distance);
      Serial.println(" cm");
    }
  } else {
    Serial.println("Not connected to WebSocket server, cannot send reading");
  }
}

bool wasConnected = false;
unsigned long lastReconnectAttempt = 0;
unsigned long lastDiscoveryCheck = 0;
unsigned long disconnectionStartTime = 0;
const unsigned long RECONNECT_INTERVAL = 3000;
const unsigned long DISCOVERY_CHECK_INTERVAL = 5000;
const unsigned long MAX_DISCONNECTION_TIME = 10000;

void loop() {
  unsigned long currentTime = millis();
  
  // Handle sensor error LED flashing
  if (sensorError) {
    // Flash the red LED when sensor error is detected
    if (currentTime - lastSensorErrorFlash >= ERROR_FLASH_INTERVAL) {
      // Toggle the red LED state
      static bool redLedState = false;
      redLedState = !redLedState;
      digitalWrite(RED_LED_PIN, redLedState ? HIGH : LOW);
      
      // Keep the green LED off during error state
      digitalWrite(GREEN_LED_PIN, LOW);
      
      lastSensorErrorFlash = currentTime;
    }
  }
  
  if (masterDiscovered) {
    webSocket.loop();
    
    bool isConnected = webSocket.isConnected();
    if (isConnected != wasConnected) {
      if (isConnected) {
        Serial.println("Connection established");
        disconnectionStartTime = 0;
      } else {
        Serial.println("Connection lost, will retry soon");
        if (disconnectionStartTime == 0) {
          disconnectionStartTime = currentTime;
          Serial.println("Starting disconnection timer");
        }
      }
      wasConnected = isConnected;
    }
    
    if (!isConnected && disconnectionStartTime > 0 && 
        (currentTime - disconnectionStartTime >= MAX_DISCONNECTION_TIME)) {
      Serial.println("Disconnected for too long (10 seconds). Going back to discovery mode");
      
      masterDiscovered = false;
      masterIP = "";
      disconnectionStartTime = 0;
      
      // Only flash discovery mode indicator if we're not in sensor error state
      if (!sensorError) {
        for (int i = 0; i < 3; i++) {
          digitalWrite(RED_LED_PIN, HIGH);
          digitalWrite(GREEN_LED_PIN, HIGH);
          delay(100);
          digitalWrite(RED_LED_PIN, LOW);
          digitalWrite(GREEN_LED_PIN, LOW);
          delay(100);
        }
      }
      
      lastDiscoveryCheck = currentTime - DISCOVERY_CHECK_INTERVAL;
    }

    else if (!isConnected && (currentTime - lastReconnectAttempt >= RECONNECT_INTERVAL)) {
      Serial.println("Attempting to reconnect...");
      lastReconnectAttempt = currentTime;

      // Only flash reconnection indicator if we're not in sensor error state
      if (!sensorError) {
        digitalWrite(GREEN_LED_PIN, HIGH);
        delay(50);
        digitalWrite(GREEN_LED_PIN, LOW);
      }
    }
    
    if (isConnected && (currentTime - lastReadingTime >= READ_INTERVAL)) {
      sendDistanceReading();
      lastReadingTime = currentTime;
    }
  } 
  else if (currentTime - lastDiscoveryCheck >= DISCOVERY_CHECK_INTERVAL) {
    Serial.println("Looking for master discovery broadcast...");
    lastDiscoveryCheck = currentTime;
    
    if (listenForDiscovery(500)) {
      connectToWebSocket();
    }
  }
  
  delay(10);
}