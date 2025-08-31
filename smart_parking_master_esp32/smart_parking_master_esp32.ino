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
WebSocketsServer webSocket = WebSocketsServer(81);
AsyncWebServer server(80);

// Constants
const int DISTANCE_THRESHOLD = 45;  // 45cm threshold for occupied status
const int MAX_DISTANCE = 200;       // Maximum reading distance
const unsigned long SYNC_INTERVAL = 5000; // 5 seconds sync interval

// Structure to hold parking spot data
struct ParkingSpot {
  int id;
  int distance;
  bool isOccupied;
  unsigned long lastUpdate;
};

// Array to store parking spots (adjust size as needed)
const int MAX_SPOTS = 10;
ParkingSpot parkingSpots[MAX_SPOTS];
int connectedSpots = 0;

// Time tracking
unsigned long lastSyncTime = 0;

// HTML for the web interface
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
      <!-- Parking spots will be added here -->
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
        // Try to reconnect after 2 seconds
        setTimeout(connect, 2000);
      };
    }
    
    function updateParkingStatus(data) {
      const statusGrid = document.getElementById('statusGrid');
      statusGrid.innerHTML = '';
      
      let availableCount = 0;
      let occupiedCount = 0;
      
      data.spots.forEach(spot => {
        const spotElement = document.createElement('div');
        spotElement.className = 'spot ' + (spot.isOccupied ? 'occupied' : 'available');
        
        const now = new Date().getTime();
        const lastUpdate = new Date(spot.lastUpdate).getTime();
        const isOffline = (now - lastUpdate) > 10000;
        
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

// Task handles for dual core operation
TaskHandle_t WebUITask;

// Function to find a parking spot by ID
int findParkingSpotById(int id) {
  for (int i = 0; i < connectedSpots; i++) {
    if (parkingSpots[i].id == id) {
      return i;
    }
  }
  return -1;
}

// Broadcast parking status to all connected clients
void broadcastParkingStatus() {
  DynamicJsonDocument doc(1024);
  JsonArray spots = doc.createNestedArray("spots");
  
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

// WebSocket event handler
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      break;
    
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[3]);
      }
      break;
    
    case WStype_TEXT:
      {
        Serial.printf("[%u] Received text: %s\n", num, payload);
        
        // Parse the incoming JSON
        DynamicJsonDocument doc(256);
        DeserializationError error = deserializeJson(doc, payload);
        
        if (error) {
          Serial.print("deserializeJson() failed: ");
          Serial.println(error.c_str());
          return;
        }
        
        // Process data from ESP8266 slave
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
          // Update spot data
          parkingSpots[spotIndex].distance = distance;
          parkingSpots[spotIndex].isOccupied = (distance <= DISTANCE_THRESHOLD && distance > 0);
          parkingSpots[spotIndex].lastUpdate = millis();
          
          // Send LED status back to the slave
          DynamicJsonDocument response(128);
          response["id"] = id;
          response["led"] = parkingSpots[spotIndex].isOccupied ? "red" : "green";
          
          String jsonResponse;
          serializeJson(response, jsonResponse);
          webSocket.sendTXT(num, jsonResponse);
          
          Serial.printf("Updated spot %d: distance=%d, occupied=%s\n", 
                       id, distance, parkingSpots[spotIndex].isOccupied ? "yes" : "no");
        }
      }
      break;
  }
}

// WebUI task - runs on core 1
void webUITaskFunction(void * parameter) {
  for(;;) {
    webSocket.loop();
    delay(1);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Master - Smart Parking System");
  
  // Configure static IP
  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("Failed to configure static IP!");
  }
  
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  // Start WebSocket server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("WebSocket server started");
  
  // Setup web server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", mainPage);
  });
  
  server.begin();
  Serial.println("HTTP server started");
  
  // Initialize parking spots
  for (int i = 0; i < MAX_SPOTS; i++) {
    parkingSpots[i].id = -1;
    parkingSpots[i].distance = 0;
    parkingSpots[i].isOccupied = false;
    parkingSpots[i].lastUpdate = 0;
  }
  
  // Start WebUI task on core 1
  xTaskCreatePinnedToCore(
    webUITaskFunction,  // Function to implement the task
    "WebUITask",        // Name of the task
    10000,              // Stack size in words
    NULL,               // Task input parameter
    1,                  // Priority of the task
    &WebUITask,         // Task handle
    1                   // Core where the task should run (1 for WebUI)
  );
  
  // Initialize sync time
  lastSyncTime = millis();
}

void loop() {
  // Main loop runs on core 0
  unsigned long currentTime = millis();
  
  // Check if it's time to sync
  if (currentTime - lastSyncTime >= SYNC_INTERVAL) {
    Serial.println("Syncing parking status...");
    
    // Broadcast current status to web clients
    broadcastParkingStatus();
    
    lastSyncTime = currentTime;
  }
  
  // Give some time to other tasks
  delay(100);
}
