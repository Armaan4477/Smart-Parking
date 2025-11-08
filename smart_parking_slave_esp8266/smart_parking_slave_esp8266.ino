#include <ESP8266WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>

// Function declarations
void webSocketEvent(WStype_t type, uint8_t* payload, size_t length);
void sendDistanceReading();

const char* ssid = "Free Public Wi-Fi";
const char* password = "2A0R0M4AAN";

String masterIP = "";
int masterPort = 81;
bool masterDiscovered = false;

// Moving average filter variables for stabilizing readings
#define READING_SAMPLES 3  // Number of samples to average
int distanceReadings[READING_SAMPLES];  // Array to store readings
int readingIndex = 0;      // Current position in the array
int readingSum = 0;        // Sum of the readings
bool filterInitialized = false; // Flag to track if the filter has been initialized

// Debouncing variables for stable state changes
#define STATE_DEBOUNCE_COUNT 2  // Number of consistent readings needed to change state
bool currentOccupiedState = false;  // Current debounced state (true = occupied)
int occupiedStateCount = 0;      // Counter for consecutive occupied readings
int availableStateCount = 0;     // Counter for consecutive available readings

int distanceThreshold = 0;
bool thresholdConfigured = false;
unsigned long lastThresholdUpdate = 0;

WiFiUDP udp;
const int UDP_PORT = 4210;
const char* DISCOVERY_MESSAGE = "SMART_PARKING";

const unsigned long DISCOVERY_TIMEOUT = 60000;
unsigned long discoveryStartTime = 0;

// Delay before starting discovery after connecting to WiFi
const unsigned long STARTUP_DELAY = 30000;
unsigned long startupTime = 0;
bool startupDelayComplete = false;

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

bool wasConnected = false;
unsigned long disconnectionStartTime = 0;
bool sensorError = false;

bool listenForDiscovery(unsigned long timeout) {
  // Make sure startup delay is complete before listening for discovery
  if (!startupDelayComplete) {
    unsigned long currentTime = millis();
    if (currentTime - startupTime < STARTUP_DELAY) {
      Serial.println("Cannot start discovery yet - still in startup delay period");
      return false;
    } else {
      startupDelayComplete = true;
    }
  }

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

      if (type && message && strcmp(type, "discovery") == 0 && strcmp(message, DISCOVERY_MESSAGE) == 0) {

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
  Serial.println("LEDs turned off until connection with master is established");

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
  
  // Set startup time and initialize startup delay
  startupTime = millis();
  startupDelayComplete = false;
  Serial.printf("Waiting %d seconds before starting discovery...\n", STARTUP_DELAY / 1000);
  
  // We'll start discovery after the delay in the loop function
  
  Serial.println("Setup complete");
}

void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("Disconnected from WebSocket server");

      thresholdConfigured = false;
      
      if (!sensorError) {
        digitalWrite(RED_LED_PIN, LOW);
        digitalWrite(GREEN_LED_PIN, LOW);

        digitalWrite(RED_LED_PIN, HIGH);
        delay(100);
        digitalWrite(RED_LED_PIN, LOW);
        
        Serial.println("LEDs turned off due to disconnection");
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

        if (!sensorError) {
          digitalWrite(RED_LED_PIN, LOW);
          
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
          
          if (doc.containsKey("threshold")) {
            int newThreshold = doc["threshold"];
            if (newThreshold > 0) {
              distanceThreshold = newThreshold;
              thresholdConfigured = true;
              Serial.printf("Received valid distance threshold: %d cm\n", distanceThreshold);
            } else {
              Serial.println("Received invalid threshold value (0 or negative). Waiting for valid threshold.");
            }
          }

          sendDistanceReading();
          return;
        }

        if (doc.containsKey("type") && strcmp(doc["type"], "id_update") == 0) {
          String macAddress = doc["mac"];
          int oldId = doc["old_id"];
          int newId = doc["new_id"];

          if (macAddress == deviceMAC) {
            if (deviceID == oldId) {
              Serial.printf("Received ID update notification: ID changed from %d to %d\n", oldId, newId);
              deviceID = newId;

              DynamicJsonDocument confirmDoc(256);
              confirmDoc["type"] = "id_update_confirm";
              confirmDoc["old_id"] = oldId;
              confirmDoc["new_id"] = newId;
              confirmDoc["mac"] = deviceMAC;

              String confirmJson;
              serializeJson(confirmDoc, confirmJson);
              webSocket.sendTXT(confirmJson);

              if (!sensorError) {
                for (int i = 0; i < 2; i++) {
                  digitalWrite(GREEN_LED_PIN, HIGH);
                  digitalWrite(RED_LED_PIN, HIGH);
                  delay(100);
                  digitalWrite(GREEN_LED_PIN, LOW);
                  digitalWrite(RED_LED_PIN, LOW);
                  delay(100);
                }
              }

              sendDistanceReading();
            }
          }
          return;
        }

        if (doc.containsKey("threshold") && doc.containsKey("id")) {
          int receivedId = doc["id"];
          if (receivedId == deviceID) {
            int newThreshold = doc["threshold"];
            if (newThreshold > 0) {
              if (distanceThreshold != newThreshold) {
                distanceThreshold = newThreshold;
                thresholdConfigured = true;
                lastThresholdUpdate = millis();
                
                Serial.printf("Updated distance threshold: %d cm\n", distanceThreshold);
                
                DynamicJsonDocument confirmDoc(256);
                confirmDoc["type"] = "threshold_confirm";
                confirmDoc["id"] = deviceID;
                confirmDoc["threshold"] = distanceThreshold;
                confirmDoc["mac"] = deviceMAC;
                
                String confirmJson;
                serializeJson(confirmDoc, confirmJson);
                webSocket.sendTXT(confirmJson);
                
                Serial.println("Sent threshold confirmation to master");
              } else {
                Serial.printf("Threshold already set to %d cm, no change needed\n", distanceThreshold);
              }
              
              if (thresholdConfigured && !sensorError && distance > 0) {
                bool isOccupied = (distance <= distanceThreshold);
                digitalWrite(RED_LED_PIN, isOccupied ? HIGH : LOW);
                digitalWrite(GREEN_LED_PIN, isOccupied ? LOW : HIGH);
                Serial.println(isOccupied ? "Updated to OCCUPIED (red LED on)" : "Updated to AVAILABLE (green LED on)");
              }
            } else {
              Serial.println("Received invalid threshold value (0 or negative). Ignoring update.");
            }
          }
        }
        
        else if (doc.containsKey("id") && doc.containsKey("led") && doc.containsKey("override")) {
          int receivedId = doc["id"];
          if (receivedId == deviceID) {
            String ledStatus = doc["led"];
            bool isOverride = doc["override"];
            
            if (isOverride && !sensorError) {
              if (ledStatus == "red") {
                digitalWrite(RED_LED_PIN, HIGH);
                digitalWrite(GREEN_LED_PIN, LOW);
                Serial.println("OVERRIDE: Setting RED LED ON (spot occupied)");
              } else if (ledStatus == "green") {
                digitalWrite(RED_LED_PIN, LOW);
                digitalWrite(GREEN_LED_PIN, HIGH);
                Serial.println("OVERRIDE: Setting GREEN LED ON (spot available)");
              }
            }
          }
        }
      }
      break;
  }
}

const int MAX_SENSOR_ERROR_COUNT = 3;
int sensorErrorCount = 0;
unsigned long lastSensorErrorFlash = 0;
const unsigned long ERROR_FLASH_INTERVAL = 300;

// Function to get a filtered distance reading
int readDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  duration = pulseIn(ECHO_PIN, HIGH, 23000);

  if (duration == 0) {
    Serial.println("WARNING: No echo received from ultrasonic sensor - possible hardware error");
    sensorErrorCount++;

    if (sensorErrorCount >= MAX_SENSOR_ERROR_COUNT) {
      sensorError = true;
    }

    return -1;
  }

  sensorErrorCount = 0;
  sensorError = false;

  int calculatedDistance = duration * 0.034 / 2;

  if (calculatedDistance > 200) {
    calculatedDistance = 200;
  }
  
  // Apply moving average filter
  // First, subtract the old reading from the sum (if filter is initialized)
  if (filterInitialized) {
    readingSum = readingSum - distanceReadings[readingIndex];
  } else {
    // If this is first time, initialize all readings with the current value
    for (int i = 0; i < READING_SAMPLES; i++) {
      distanceReadings[i] = calculatedDistance;
    }
    readingSum = calculatedDistance * READING_SAMPLES;
    filterInitialized = true;
  }
  
  // Add new reading to the sum and store in array
  readingSum = readingSum + calculatedDistance;
  distanceReadings[readingIndex] = calculatedDistance;
  
  // Move to next position in circular buffer
  readingIndex = (readingIndex + 1) % READING_SAMPLES;
  
  // Calculate average
  int filteredDistance = readingSum / READING_SAMPLES;
  
  Serial.print("Raw distance: "); 
  Serial.print(calculatedDistance);
  Serial.print(" cm, Filtered: ");
  Serial.print(filteredDistance);
  Serial.println(" cm");
  
  return filteredDistance;
}

void sendDistanceReading() {
  if (webSocket.isConnected()) {
    if (deviceID < 0) {
      Serial.println("No valid device ID assigned yet. Cannot send reading.");
      return;
    }

    distance = readDistance();

    if (distance == -1) {
      Serial.println("ERROR: Failed to get valid distance reading");

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
      bool rawOccupiedState = false;
      bool reportedOccupiedState = currentOccupiedState; // Default to current stable state
      
      if (thresholdConfigured) {
        // Determine the raw state based on the filtered distance reading
        rawOccupiedState = (distance <= distanceThreshold && distance > 0);
        
        // Apply debounce logic for state changes
        if (rawOccupiedState) {
          // Object detected - increment occupied counter, reset available counter
          occupiedStateCount++;
          availableStateCount = 0;
          
          // If we have enough consecutive occupied readings, change state
          if (occupiedStateCount >= STATE_DEBOUNCE_COUNT && !currentOccupiedState) {
            currentOccupiedState = true;
            reportedOccupiedState = true;
            Serial.println("State change debounced: Now OCCUPIED");
          }
        } else {
          // No object detected - increment available counter, reset occupied counter
          availableStateCount++;
          occupiedStateCount = 0;
          
          // If we have enough consecutive available readings, change state
          if (availableStateCount >= STATE_DEBOUNCE_COUNT && currentOccupiedState) {
            currentOccupiedState = false;
            reportedOccupiedState = false;
            Serial.println("State change debounced: Now AVAILABLE");
          }
        }
        
        if (!sensorError) {
          digitalWrite(RED_LED_PIN, reportedOccupiedState ? HIGH : LOW);
          digitalWrite(GREEN_LED_PIN, reportedOccupiedState ? LOW : HIGH);
        }
        
        Serial.print("Raw state: ");
        Serial.print(rawOccupiedState ? "OCCUPIED" : "AVAILABLE");
        Serial.print(", Debounced state: ");
        Serial.println(reportedOccupiedState ? "OCCUPIED (red LED on)" : "AVAILABLE (green LED on)");
        Serial.printf("Debounce counters - Occupied: %d, Available: %d\n", occupiedStateCount, availableStateCount);
      } else {
        if (!sensorError) {
          digitalWrite(RED_LED_PIN, LOW);
          digitalWrite(GREEN_LED_PIN, LOW);
        }
        Serial.println("Threshold not configured yet. LEDs turned off.");
      }

      DynamicJsonDocument doc(256);
      doc["id"] = deviceID;
      doc["mac"] = deviceMAC;
      doc["distance"] = distance;
      
      if (thresholdConfigured) {
        doc["isOccupied"] = reportedOccupiedState; // Use the debounced state
        doc["threshold"] = distanceThreshold;
      }

      String jsonString;
      serializeJson(doc, jsonString);
      webSocket.sendTXT(jsonString);

      Serial.print("Sent distance reading: ");
      Serial.print(distance);
      Serial.print(" cm, Debounced Occupied: ");
      Serial.println(reportedOccupiedState ? "yes" : "no");
    }
  } else {
    Serial.println("Not connected to WebSocket server, cannot send reading");
  }
}

unsigned long lastReconnectAttempt = 0;
unsigned long lastDiscoveryCheck = 0;
const unsigned long RECONNECT_INTERVAL = 3000;
const unsigned long DISCOVERY_CHECK_INTERVAL = 5000;
const unsigned long MAX_DISCONNECTION_TIME = 10000;

void loop() {
  unsigned long currentTime = millis();

  // Check if startup delay is complete before attempting to discover master
  if (!startupDelayComplete) {
    if (currentTime - startupTime >= STARTUP_DELAY) {
      startupDelayComplete = true;
      Serial.println("Startup delay complete! Starting discovery process...");
      
      // Start the discovery process
      discoveryStartTime = currentTime;
      if (listenForDiscovery(DISCOVERY_TIMEOUT)) {
        connectToWebSocket();
      } else {
        Serial.println("No master found, will keep trying");
      }
    } else {
      // Print status message every 5 seconds during startup delay
      unsigned long remainingTime = STARTUP_DELAY - (currentTime - startupTime);
      if (currentTime % 5000 < 100) {
        Serial.printf("Startup delay in progress. %lu seconds remaining...\n", remainingTime / 1000);
      }
      delay(100); // Small delay to avoid flooding the serial console
      return; // Skip the rest of the loop during startup delay
    }
  }

  if (sensorError) {
    if (currentTime - lastSensorErrorFlash >= ERROR_FLASH_INTERVAL) {
      static bool redLedState = false;
      redLedState = !redLedState;
      digitalWrite(RED_LED_PIN, redLedState ? HIGH : LOW);

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
        
        if (wasConnected) {
          thresholdConfigured = false;
          
          if (!sensorError) {
            digitalWrite(RED_LED_PIN, LOW);
            digitalWrite(GREEN_LED_PIN, LOW);
          }
        }
      }
      wasConnected = isConnected;
    }

    if (!isConnected && disconnectionStartTime > 0 && (currentTime - disconnectionStartTime >= MAX_DISCONNECTION_TIME)) {
      Serial.println("Disconnected for too long (10 seconds). Going back to discovery mode");

      masterDiscovered = false;
      masterIP = "";
      disconnectionStartTime = 0;

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

      if (!sensorError) {
        digitalWrite(GREEN_LED_PIN, HIGH);
        delay(50);
        digitalWrite(GREEN_LED_PIN, LOW);
      }
    }

    if (isConnected && (currentTime - lastReadingTime >= READ_INTERVAL)) {
      sendDistanceReading();
      lastReadingTime = currentTime;
      
      if (!thresholdConfigured && (currentTime % 10000 < 100)) { // Log roughly every 10 seconds
        Serial.println("Connected to master but no valid threshold configured yet.");
      }
    }
  } else if (startupDelayComplete && (currentTime - lastDiscoveryCheck >= DISCOVERY_CHECK_INTERVAL)) {
    Serial.println("Looking for master discovery broadcast...");
    lastDiscoveryCheck = currentTime;

    if (listenForDiscovery(500)) {
      connectToWebSocket();
    }
  }

  delay(10);
}