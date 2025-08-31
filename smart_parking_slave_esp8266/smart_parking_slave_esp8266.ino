#include <ESP8266WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

const char* ssid = "Free Public Wi-Fi";
const char* password = "2A0R0M4AAN";

// Master ESP32 IP and WebSocket port (hardcoded)
const char* masterIP = "192.168.29.30";
const int masterPort = 81;

// Device ID (hardcoded for each slave - change this for each device)
const int DEVICE_ID = 4;  // Unique ID for this slave

// Ultrasonic sensor pins
const int TRIG_PIN = 5;  // Trigger pin connected to D1
const int ECHO_PIN = 4;  // Echo pin connected to D2

// LED pins
const int RED_LED_PIN = 2;    // D4
const int GREEN_LED_PIN = 0;  // D3

// WebSocket client
WebSocketsClient webSocket;

// Variables for distance measurement
long duration;
int distance;
unsigned long lastReadingTime = 0;
const unsigned long READ_INTERVAL = 2000;  // Read every 2 seconds

void setup() {
  Serial.begin(115200);
  Serial.println("ESP8266 Slave - Smart Parking System");

  // Initialize LED pins
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, LOW);
  
  // Initialize ultrasonic sensor pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Connect to Wi-Fi
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
  
  // Set up WebSocket connection to master
  webSocket.begin(masterIP, masterPort, "/");
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);  // Try to reconnect every 5 seconds if connection is lost
  
  Serial.println("WebSocket connection initialized");
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("Disconnected from WebSocket server");
      // When disconnected, both LEDs off
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(GREEN_LED_PIN, LOW);
      break;
      
    case WStype_CONNECTED:
      Serial.println("Connected to WebSocket server");
      // Send initial reading when connected
      sendDistanceReading();
      break;
      
    case WStype_TEXT:
      Serial.printf("Received text: %s\n", payload);
      
      // Parse the incoming JSON
      DynamicJsonDocument doc(128);
      DeserializationError error = deserializeJson(doc, payload);
      
      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return;
      }
      
      // Check if message is for this slave
      int receivedId = doc["id"];
      if (receivedId == DEVICE_ID) {
        String ledStatus = doc["led"];
        
        if (ledStatus == "red") {
          // Occupied status - turn on red LED, turn off green LED
          digitalWrite(RED_LED_PIN, HIGH);
          digitalWrite(GREEN_LED_PIN, LOW);
          Serial.println("Setting RED LED ON (spot occupied)");
        } else if (ledStatus == "green") {
          // Available status - turn on green LED, turn off red LED
          digitalWrite(RED_LED_PIN, LOW);
          digitalWrite(GREEN_LED_PIN, HIGH);
          Serial.println("Setting GREEN LED ON (spot available)");
        }
      }
      break;
  }
}

// Function to read distance from ultrasonic sensor
int readDistance() {
  // Clear the trigger pin
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  
  // Set the trigger pin HIGH for 10 microseconds
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  // Read the echo pin, returns the sound wave travel time in microseconds
  duration = pulseIn(ECHO_PIN, HIGH);
  
  // Calculate the distance
  int calculatedDistance = duration * 0.034 / 2;  // Speed of sound wave divided by 2 (go and back)
  
  // Cap the distance at max reading
  if (calculatedDistance > 200) {
    calculatedDistance = 200;
  }
  
  return calculatedDistance;
}

// Function to send distance reading to master
void sendDistanceReading() {
  if (webSocket.isConnected()) {
    distance = readDistance();
    
    // Prepare JSON message
    DynamicJsonDocument doc(128);
    doc["id"] = DEVICE_ID;
    doc["distance"] = distance;
    
    // Serialize and send
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

void loop() {
  webSocket.loop();
  
  unsigned long currentTime = millis();
  
  // Read and send distance at regular intervals
  if (currentTime - lastReadingTime >= READ_INTERVAL) {
    sendDistanceReading();
    lastReadingTime = currentTime;
  }
}
