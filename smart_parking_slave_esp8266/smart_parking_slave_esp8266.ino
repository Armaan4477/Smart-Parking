#include <ESP8266WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

const char* ssid = "Free Public Wi-Fi";
const char* password = "2A0R0M4AAN";

const char* masterIP = "192.168.29.30";
const int masterPort = 81;

const int DEVICE_ID = 1;  // Unique ID for this slave

const int TRIG_PIN = 5;  // Trigger pin connected to D1
const int ECHO_PIN = 4;  // Echo pin connected to D2

// LED pins
const int RED_LED_PIN = 2;    // D4
const int GREEN_LED_PIN = 0;  // D3

WebSocketsClient webSocket;

long duration;
int distance;
unsigned long lastReadingTime = 0;
const unsigned long READ_INTERVAL = 2000;

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
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  int connectionDelay = 1000 + (DEVICE_ID * 500);
  Serial.print("Waiting ");
  Serial.print(connectionDelay);
  Serial.println("ms before connecting to WebSocket server...");
  delay(connectionDelay);
  
  webSocket.begin(masterIP, masterPort, "/");
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
  
  Serial.println("WebSocket connection initialized");
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("Disconnected from WebSocket server");
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(GREEN_LED_PIN, LOW);
      
      digitalWrite(RED_LED_PIN, HIGH);
      delay(100);
      digitalWrite(RED_LED_PIN, LOW);
      break;
      
    case WStype_CONNECTED:
      Serial.println("Connected to WebSocket server");

      digitalWrite(GREEN_LED_PIN, HIGH);
      delay(100);
      digitalWrite(GREEN_LED_PIN, LOW);
      delay(100);
      digitalWrite(GREEN_LED_PIN, HIGH);
      delay(100);
      digitalWrite(GREEN_LED_PIN, LOW);
      
      sendDistanceReading();
      break;
      
    case WStype_TEXT:
      Serial.printf("Received text: %s\n", payload);
      
      DynamicJsonDocument doc(128);
      DeserializationError error = deserializeJson(doc, payload);
      
      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return;
      }
      
      int receivedId = doc["id"];
      if (receivedId == DEVICE_ID) {
        String ledStatus = doc["led"];
        
        if (ledStatus == "red") {
          digitalWrite(RED_LED_PIN, HIGH);
          digitalWrite(GREEN_LED_PIN, LOW);
          Serial.println("Setting RED LED ON (spot occupied)");
        } else if (ledStatus == "green") {
          digitalWrite(RED_LED_PIN, LOW);
          digitalWrite(GREEN_LED_PIN, HIGH);
          Serial.println("Setting GREEN LED ON (spot available)");
        }
      }
      break;
  }
}

int readDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  duration = pulseIn(ECHO_PIN, HIGH);
  
  int calculatedDistance = duration * 0.034 / 2;
  
  if (calculatedDistance > 200) {
    calculatedDistance = 200;
  }
  
  return calculatedDistance;
}

void sendDistanceReading() {
  if (webSocket.isConnected()) {
    distance = readDistance();
    
    DynamicJsonDocument doc(128);
    doc["id"] = DEVICE_ID;
    doc["distance"] = distance;
    
    String jsonString;
    serializeJson(doc, jsonString);
    webSocket.sendTXT(jsonString);
    
    Serial.print("Sent distance reading: ");
    Serial.print(distance);
    Serial.println(" cm");
  } else {
    Serial.println("Not connected to WebSocket server, cannot send reading");
  }
}

bool wasConnected = false;
unsigned long lastReconnectAttempt = 0;
const unsigned long RECONNECT_INTERVAL = 3000;

void loop() {
  webSocket.loop();
  
  unsigned long currentTime = millis();
  
  bool isConnected = webSocket.isConnected();
  if (isConnected != wasConnected) {
    if (isConnected) {
      Serial.println("Connection established");
    } else {
      Serial.println("Connection lost, will retry soon");
    }
    wasConnected = isConnected;
  }
  
  if (!isConnected && (currentTime - lastReconnectAttempt >= RECONNECT_INTERVAL)) {
    Serial.println("Attempting to reconnect...");
    lastReconnectAttempt = currentTime;

    digitalWrite(GREEN_LED_PIN, HIGH);
    delay(50);
    digitalWrite(GREEN_LED_PIN, LOW);
  }
  
  if (isConnected && (currentTime - lastReadingTime >= READ_INTERVAL)) {
    sendDistanceReading();
    lastReadingTime = currentTime;
  }
}
