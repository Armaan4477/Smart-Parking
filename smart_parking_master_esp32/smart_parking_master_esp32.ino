#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

const char* ssid = "Free Public Wi-Fi";
const char* password = "2A0R0M4AAN";

// IP address for ESP32 (hardcoded)
IPAddress local_IP(192, 168, 29, 30);
IPAddress gateway(192, 168, 29, 1);
IPAddress subnet(255, 255, 255, 0);

// WebSocket server
WebSocketsServer webSocket = WebSocketsServer(81, "", "arduino");
AsyncWebServer server(80);

// Constants
const int DISTANCE_THRESHOLD = 45;
const int MAX_DISTANCE = 200;
const unsigned long SYNC_INTERVAL = 5000;

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
        updateParkingStatus(JSON.parse(event.data));
      };
      
      socket.onclose = function() {
        console.log('WebSocket connection closed');
        setTimeout(connect, 2000);
      };
    }
    
    function updateParkingStatus(data) {
      const statusGrid = document.getElementById('statusGrid');
      statusGrid.innerHTML = '';
      
      let availableCount = 0;
      let occupiedCount = 0;
      
      console.log("Received parking data:", data);
      
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
    
    // Connect when page loads
    window.onload = connect;
  </script>
</body>
</html>
)html";

TaskHandle_t WebUITask;

int findParkingSpotById(int id) {
  for (int i = 0; i < connectedSpots; i++) {
    if (parkingSpots[i].id == id) {
      return i;
    }
  }
  return -1;
}

void broadcastParkingStatus() {
  DynamicJsonDocument doc(1024);
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
        
        int id = doc["id"];
        int distance = doc["distance"];
        
        int spotIndex = findParkingSpotById(id);
        if (spotIndex == -1 && connectedSpots < MAX_SPOTS) {
          // New spot
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
      break;
  }
}

void webUITaskFunction(void * parameter) {
  for(;;) {
    webSocket.loop();
    delay(1);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Master - Smart Parking System");
  
  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("Failed to configure static IP!");
  }
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

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
  
  lastSyncTime = millis();
}

void loop() {
  unsigned long currentTime = millis();
  
  if (currentTime - lastSyncTime >= SYNC_INTERVAL) {
    Serial.println("Syncing parking status...");
    
    broadcastParkingStatus();
    
    lastSyncTime = currentTime;
  }
  
  delay(100);
}
