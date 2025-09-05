#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>

const char* ssid = "JoeMama";
const char* password = "2A0R0M4AAN";

WiFiUDP udp;
const int UDP_PORT = 4210;
const char* DISCOVERY_MESSAGE = "SMART_PARKING";

const int DISCOVERY_BUTTON_PIN = 32;
const int DISCOVERY_LED_PIN = 33;
bool buttonPressed = false;
bool lastButtonState = HIGH;
bool physicalOverrideActive = false;

WebSocketsServer webSocket = WebSocketsServer(81, "", "arduino");
AsyncWebServer server(80);

const int DISTANCE_THRESHOLD = 45;
const int MAX_DISTANCE = 200;
const unsigned long SYNC_INTERVAL = 5000;

const unsigned long DISCOVERY_DURATION = 60000;
const unsigned long DISCOVERY_INTERVAL = 5000;
unsigned long discoveryStartTime = 0;
unsigned long lastDiscoveryTime = 0;
bool discoveryMode = false;

struct ParkingSpot {
  int id;
  int distance;
  bool isOccupied;
  unsigned long lastUpdate;
};

const int MAX_SPOTS = 10;
ParkingSpot parkingSpots[MAX_SPOTS];
int connectedSpots = 0;

unsigned long lastSyncTime = 0;

const char mainPage[] PROGMEM = R"html(
<!DOCTYPE html>
<html>
<head>
  <title>Smart Parking System</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body {
      font-family: Arial, sans-serif;
      margin: 0;
      padding: 20px;
      background-color: #f4f4f4;
    }
    .container {
      max-width: 800px;
      margin: 0 auto;
      background-color: white;
      padding: 20px;
      border-radius: 8px;
      box-shadow: 0 2px 10px rgba(0,0,0,0.1);
    }
    h1 {
      color: #333;
      text-align: center;
    }
    .status-grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(200px, 1fr));
      gap: 15px;
      margin-top: 20px;
    }
    .spot {
      border-radius: 8px;
      padding: 15px;
      text-align: center;
      color: white;
      font-weight: bold;
    }
    .occupied {
      background-color: #d9534f;
    }
    .available {
      background-color: #5cb85c;
    }
    .offline {
      background-color: #f0ad4e;
    }
    .stats {
      margin-top: 20px;
      padding: 15px;
      background-color: #eaeaea;
      border-radius: 8px;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>Smart Parking System</h1>
    <div class="stats">
      <p>Total Spots: <span id="totalSpots">0</span></p>
      <p>Available Spots: <span id="availableSpots">0</span></p>
      <p>Occupied Spots: <span id="occupiedSpots">0</span></p>
      <div style="display: flex; gap: 10px;">
        <button id="startDiscoveryBtn" style="margin-top: 10px; padding: 8px 15px; background-color: #007bff; color: white; border: none; border-radius: 4px; cursor: pointer;">Start Discovery</button>
        <button id="stopDiscoveryBtn" style="margin-top: 10px; padding: 8px 15px; background-color: #dc3545; color: white; border: none; border-radius: 4px; cursor: pointer;">Stop Discovery</button>
      </div>
      <span id="discoveryStatus" style="margin-left: 10px; display: none;">Discovery mode active...</span>
    </div>
    <div id="statusGrid" class="status-grid">
    </div>
  </div>

  <script>
    var socket;
    
    function connect() {
      socket = new WebSocket('ws://' + window.location.hostname + ':81/');
      
      socket.onopen = function() {
        console.log('WebSocket connection established');
      };
      
      socket.onmessage = function(event) {
        const data = JSON.parse(event.data);
        
        if (data.type === 'discovery_status') {
          handleDiscoveryStatus(data);
        } else if (data.spots) {
          updateParkingStatus(data);
        } else {
          console.log('Received other message:', data);
        }
      };
      
      socket.onclose = function() {
        console.log('WebSocket connection closed');
        setTimeout(connect, 2000);
      };
      
      document.getElementById('startDiscoveryBtn').addEventListener('click', function() {
        if (socket && socket.readyState === WebSocket.OPEN) {
          const message = JSON.stringify({ command: 'start_discovery' });
          socket.send(message);
          console.log('Start discovery request sent');
        }
      });
      
      document.getElementById('stopDiscoveryBtn').addEventListener('click', function() {
        if (socket && socket.readyState === WebSocket.OPEN) {
          const message = JSON.stringify({ command: 'stop_discovery' });
          socket.send(message);
          console.log('Stop discovery request sent');
        }
      });
    }
    
    function handleDiscoveryStatus(data) {
      console.log('Discovery status:', data);
      const statusEl = document.getElementById('discoveryStatus');
      
      if (data.active) {
        if (data.physical_override) {
          statusEl.textContent = 'Discovery mode active (Physical button override)';
          statusEl.style.color = '#007bff'; // Blue color for physical override
          document.getElementById('startDiscoveryBtn').disabled = true;
          document.getElementById('stopDiscoveryBtn').disabled = true; // Disable both buttons during physical override
        } else {
          statusEl.textContent = 'Discovery mode active...';
          statusEl.style.color = '#28a745'; // Green color for software active status
          document.getElementById('startDiscoveryBtn').disabled = true;
          document.getElementById('stopDiscoveryBtn').disabled = false;
        }
        statusEl.style.display = 'inline';
      } else {
        statusEl.textContent = 'Discovery mode stopped';
        statusEl.style.display = 'inline';
        statusEl.style.color = '#dc3545'; // Red color for inactive status
        setTimeout(function() {
          statusEl.style.display = 'none';
        }, 3000);
        document.getElementById('startDiscoveryBtn').disabled = false;
        document.getElementById('stopDiscoveryBtn').disabled = true;
      }
      
      if (data.message) {
        console.log('Status message:', data.message);
      }
    }
    
    function updateParkingStatus(data) {
      const statusGrid = document.getElementById('statusGrid');
      statusGrid.innerHTML = '';
      
      let availableCount = 0;
      let occupiedCount = 0;
      
      console.log("Received parking data:", data);
      
      if (!data.spots || !Array.isArray(data.spots)) {
        console.error("Invalid parking data format: missing spots array");
        return;
      }
      
      data.spots.forEach(spot => {
        const spotElement = document.createElement('div');
        spotElement.className = 'spot ' + (spot.isOccupied ? 'occupied' : 'available');
        
        const currentServerTime = data.serverTime || 0;
        const timeSinceUpdate = currentServerTime - spot.lastUpdate;
        const isOffline = currentServerTime > 0 && timeSinceUpdate > 10000;
        
        console.log(`Spot ${spot.id}: Time since last update: ${timeSinceUpdate}ms, Status: ${isOffline ? 'Offline' : (spot.isOccupied ? 'Occupied' : 'Available')}`);
        
        if (isOffline) {
          spotElement.className = 'spot offline';
          spotElement.textContent = `Spot ${spot.id}: Offline`;
        } else {
          if (spot.isOccupied) {
            occupiedCount++;
            spotElement.textContent = `Spot ${spot.id}: Occupied`;
          } else {
            availableCount++;
            spotElement.textContent = `Spot ${spot.id}: Available`;
          }
        }
        
        statusGrid.appendChild(spotElement);
      });
      
      document.getElementById('totalSpots').textContent = data.spots.length;
      document.getElementById('availableSpots').textContent = availableCount;
      document.getElementById('occupiedSpots').textContent = occupiedCount;
    }
    
    window.onload = function() {
      connect();
      document.getElementById('startDiscoveryBtn').disabled = false;
      document.getElementById('stopDiscoveryBtn').disabled = true;
    };
  </script>
</body>
</html>
)html";

TaskHandle_t WebUITask;
TaskHandle_t SyncTask;

int findParkingSpotById(int id) {
  for (int i = 0; i < connectedSpots; i++) {
    if (parkingSpots[i].id == id) {
      return i;
    }
  }
  return -1;
}

void broadcastDiscovery() {
  udp.beginPacket(IPAddress(255, 255, 255, 255), UDP_PORT);
  
  DynamicJsonDocument discoveryDoc(128);
  discoveryDoc["type"] = "discovery";
  discoveryDoc["message"] = DISCOVERY_MESSAGE;
  discoveryDoc["ip"] = WiFi.localIP().toString();
  discoveryDoc["port"] = 81;
  
  String discoveryJson;
  serializeJson(discoveryDoc, discoveryJson);
  
  udp.write((const uint8_t*)discoveryJson.c_str(), discoveryJson.length());
  udp.endPacket();
  
  Serial.println("Discovery broadcast sent: " + discoveryJson);
}

void broadcastParkingStatus() {
  DynamicJsonDocument doc(1024);
  doc["type"] = "parking_status";
  JsonArray spots = doc.createNestedArray("spots");
  
  doc["serverTime"] = millis();
  
  for (int i = 0; i < connectedSpots; i++) {
    JsonObject spot = spots.createNestedObject();
    spot["id"] = parkingSpots[i].id;
    spot["distance"] = parkingSpots[i].distance;
    spot["isOccupied"] = parkingSpots[i].isOccupied;
    spot["lastUpdate"] = parkingSpots[i].lastUpdate;
  }
  
  String jsonString;
  serializeJson(doc, jsonString);
  webSocket.broadcastTXT(jsonString);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      delay(50);
      break;
    
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[3]);
        
        delay(100);
        
        DynamicJsonDocument statusDoc(128);
        statusDoc["type"] = "discovery_status";
        statusDoc["active"] = discoveryMode;
        
        String statusJson;
        serializeJson(statusDoc, statusJson);
        webSocket.sendTXT(num, statusJson);
        
        broadcastParkingStatus();
      }
      break;
    
    case WStype_TEXT:
      {
        Serial.printf("[%u] Received text: %s\n", num, payload);
        
        DynamicJsonDocument doc(256);
        DeserializationError error = deserializeJson(doc, payload);
        
        if (error) {
          Serial.print("deserializeJson() failed: ");
          Serial.println(error.c_str());
          return;
        }
        
        const char* command = doc["command"];
        if (command && strcmp(command, "start_discovery") == 0) {
          Serial.println("Start discovery command received from web interface");
          
          discoveryMode = true;
          discoveryStartTime = millis();
          lastDiscoveryTime = 0;
          digitalWrite(DISCOVERY_LED_PIN, HIGH);
          
          DynamicJsonDocument responseDoc(128);
          responseDoc["type"] = "discovery_status";
          responseDoc["active"] = true;
          
          String responseJson;
          serializeJson(responseDoc, responseJson);
          webSocket.broadcastTXT(responseJson);
          
          return;
        }
        
        if (command && strcmp(command, "stop_discovery") == 0) {
          Serial.println("Stop discovery command received from web interface");
          
          if (!physicalOverrideActive) {
            discoveryMode = false;
            digitalWrite(DISCOVERY_LED_PIN, LOW);
            
            DynamicJsonDocument responseDoc(128);
            responseDoc["type"] = "discovery_status";
            responseDoc["active"] = false;
            responseDoc["physical_override"] = false;
            
            String responseJson;
            serializeJson(responseDoc, responseJson);
            webSocket.broadcastTXT(responseJson);
          } else {
            Serial.println("Cannot stop discovery: Physical override is active");
            
            DynamicJsonDocument responseDoc(128);
            responseDoc["type"] = "discovery_status";
            responseDoc["active"] = true;
            responseDoc["physical_override"] = true;
            responseDoc["message"] = "Cannot stop discovery while physical button is pressed";
            
            String responseJson;
            serializeJson(responseDoc, responseJson);
            webSocket.sendTXT(num, responseJson);
          }
          
          return;
        }
        
        if (doc.containsKey("id") && doc.containsKey("distance")) {
          int id = doc["id"];
          int distance = doc["distance"];
          
          int spotIndex = findParkingSpotById(id);
          if (spotIndex == -1 && connectedSpots < MAX_SPOTS) {
            spotIndex = connectedSpots;
            parkingSpots[spotIndex].id = id;
            connectedSpots++;
            Serial.printf("New parking spot added: ID %d\n", id);
          }
          
          if (spotIndex != -1) {
            bool wasOccupied = parkingSpots[spotIndex].isOccupied;
            
            parkingSpots[spotIndex].distance = distance;
            parkingSpots[spotIndex].isOccupied = (distance <= DISTANCE_THRESHOLD && distance > 0);
            parkingSpots[spotIndex].lastUpdate = millis();
            
            DynamicJsonDocument response(128);
            response["id"] = id;
            response["led"] = parkingSpots[spotIndex].isOccupied ? "red" : "green";
            
            String jsonResponse;
            serializeJson(response, jsonResponse);
            webSocket.sendTXT(num, jsonResponse);
            
            Serial.printf("Updated spot %d: distance=%d, occupied=%s\n", 
                         id, distance, parkingSpots[spotIndex].isOccupied ? "yes" : "no");
            
            if (wasOccupied != parkingSpots[spotIndex].isOccupied) {
              Serial.println("Spot status changed, broadcasting update to all clients");
              broadcastParkingStatus();
            }
          }
        }
      }
      break;
  }
}

void webUITaskFunction(void * parameter) {
  for(;;) {
    webSocket.loop();
    delay(1);
  }
}

void syncTaskFunction(void * parameter) {
  unsigned long lastSyncTime = 0;
  unsigned long lastDiscoveryTime = 0;
  
  for(;;) {
    unsigned long currentTime = millis();
    
    if (discoveryMode) {
      if (physicalOverrideActive || (currentTime - discoveryStartTime < DISCOVERY_DURATION)) {
        if (currentTime - lastDiscoveryTime >= DISCOVERY_INTERVAL) {
          broadcastDiscovery();
          lastDiscoveryTime = currentTime;
        }
      } else {
        Serial.println("Discovery mode ended due to timeout");
        discoveryMode = false;
        digitalWrite(DISCOVERY_LED_PIN, LOW);
        
        DynamicJsonDocument responseDoc(128);
        responseDoc["type"] = "discovery_status";
        responseDoc["active"] = false;
        responseDoc["physical_override"] = false;
        
        String responseJson;
        serializeJson(responseDoc, responseJson);
        webSocket.broadcastTXT(responseJson);
      }
    }
    
    if (currentTime - lastSyncTime >= SYNC_INTERVAL) {
      Serial.println("Syncing parking status...");
      broadcastParkingStatus();
      lastSyncTime = currentTime;
    }
    
    delay(100);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Master - Smart Parking System");
  
  pinMode(DISCOVERY_BUTTON_PIN, INPUT_PULLUP);
  pinMode(DISCOVERY_LED_PIN, OUTPUT);
  digitalWrite(DISCOVERY_LED_PIN, LOW);
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  udp.begin(UDP_PORT);
  Serial.print("UDP Discovery Service started on port ");
  Serial.println(UDP_PORT);
  
  discoveryMode = true;
  discoveryStartTime = millis();
  lastDiscoveryTime = 0;

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("WebSocket server started");
  

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", mainPage);
  });
  
  server.begin();
  Serial.println("HTTP server started");
  
  for (int i = 0; i < MAX_SPOTS; i++) {
    parkingSpots[i].id = -1;
    parkingSpots[i].distance = 0;
    parkingSpots[i].isOccupied = false;
    parkingSpots[i].lastUpdate = 0;
  }
  
  xTaskCreatePinnedToCore(
    webUITaskFunction,
    "WebUITask",
    10000,
    NULL,
    1,
    &WebUITask,
    1
  );
  
  xTaskCreatePinnedToCore(
    syncTaskFunction,
    "SyncTask",
    10000,
    NULL,
    1,
    &SyncTask,
    0
  );
}

void checkDiscoveryButton() {
  bool buttonState = digitalRead(DISCOVERY_BUTTON_PIN);
  
  if (buttonState == LOW && lastButtonState == HIGH) {
    Serial.println("Discovery button pressed - physical override activated!");
    
    physicalOverrideActive = true;
    discoveryMode = true;
    discoveryStartTime = millis();
    lastDiscoveryTime = 0;
    digitalWrite(DISCOVERY_LED_PIN, HIGH);
    
    DynamicJsonDocument responseDoc(128);
    responseDoc["type"] = "discovery_status";
    responseDoc["active"] = true;
    responseDoc["physical_override"] = true;
    
    String responseJson;
    serializeJson(responseDoc, responseJson);
    webSocket.broadcastTXT(responseJson);
  }
  
  if (buttonState == HIGH && lastButtonState == LOW) {
    Serial.println("Discovery button released - physical override deactivated!");
    physicalOverrideActive = false;
    
    discoveryMode = false;
    digitalWrite(DISCOVERY_LED_PIN, LOW);
    
    DynamicJsonDocument responseDoc(128);
    responseDoc["type"] = "discovery_status";
    responseDoc["active"] = false;
    responseDoc["physical_override"] = false;
    responseDoc["message"] = "Discovery mode stopped due to button release";
    
    String responseJson;
    serializeJson(responseDoc, responseJson);
    webSocket.broadcastTXT(responseJson);
  }
  
  lastButtonState = buttonState;
}

void loop() {
  unsigned long currentTime = millis();

  checkDiscoveryButton();
  
  bool buttonState = digitalRead(DISCOVERY_BUTTON_PIN);
  
  if (buttonState == LOW) {
    physicalOverrideActive = true;
    discoveryMode = true;
  } else {
    physicalOverrideActive = false;
  }
  
  if (discoveryMode) {
    if (physicalOverrideActive) {
      if (currentTime % 300 < 150) {
        digitalWrite(DISCOVERY_LED_PIN, HIGH);
      } else {
        digitalWrite(DISCOVERY_LED_PIN, LOW);
      }
    } else {
      if (currentTime % 500 < 250) {
        digitalWrite(DISCOVERY_LED_PIN, HIGH);
      } else {
        digitalWrite(DISCOVERY_LED_PIN, LOW);
      }
    }
  } else {
    digitalWrite(DISCOVERY_LED_PIN, LOW);
  }
  
  delay(50);
}
