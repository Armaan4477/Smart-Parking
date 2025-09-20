#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <Preferences.h>
#include <HTTPClient.h>  // Added for API communication

const char* ssid = "JoeMama";
const char* password = "2A0R0M4AAN";

// API Configuration - Update with your actual API URL
const char* API_BASE_URL = "http://your-api-url.com/api/parking";  // Replace with your actual deployed API URL

WiFiUDP udp;
const int UDP_PORT = 4210;
const char* DISCOVERY_MESSAGE = "SMART_PARKING";

const int DISCOVERY_BUTTON_PIN = 32;
const int DISCOVERY_LED_PIN = 33;
const int ERROR_LED_PIN = 25;
bool buttonPressed = false;
bool lastButtonState = HIGH;
bool physicalOverrideActive = false;
bool systemErrorState = false;
unsigned long lastErrorLedToggle = 0;
const unsigned long ERROR_LED_TOGGLE_INTERVAL = 250;

WebSocketsServer webSocket = WebSocketsServer(81, "", "arduino");
AsyncWebServer server(80);

const int DISTANCE_THRESHOLD = 45;
const int MAX_DISTANCE = 200;
const unsigned long SYNC_INTERVAL = 5000;
const unsigned long API_SYNC_INTERVAL = 10000;  // Interval for syncing with API (10 seconds)

const unsigned long DISCOVERY_DURATION = 60000;
const unsigned long DISCOVERY_INTERVAL = 5000;
unsigned long discoveryStartTime = 0;
unsigned long lastDiscoveryTime = 0;
bool discoveryMode = false;

const unsigned long OFFLINE_DETECTION_WINDOW = 10000;
unsigned long firstOfflineTime = 0;
int offlineCount = 0;
bool wifiDisconnectionDetected = false;

unsigned long lastApiSyncTime = 0;  // Track last time data was sent to API

struct ParkingSpot {
  int id;
  String macAddress;
  int distance;
  bool isOccupied;
  unsigned long lastUpdate;
  bool hasSensorError = false;
  bool syncedWithApi = false;  // Track if this spot has been synced with API
};

const int MAX_SPOTS = 10;
ParkingSpot parkingSpots[MAX_SPOTS];
int connectedSpots = 0;
int nextAvailableId = 1;

Preferences preferences;

unsigned long lastSyncTime = 0;

// Function declarations for API communication
void initializeSpotWithApi(int spotId);
void updateSpotStatusOnApi(int spotId);
void sendSensorDataToApi(int spotId, int distance, bool hasSensorError);
bool makeApiRequest(String endpoint, String method, String payload = "");

// Existing web page HTML
const char mainPage[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>Smart Parking System</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    /* Your existing CSS styles */
  </style>
</head>
<body>
  <!-- Your existing HTML content -->
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Master - Smart Parking System");

  // Initialize pins
  pinMode(DISCOVERY_BUTTON_PIN, INPUT_PULLUP);
  pinMode(DISCOVERY_LED_PIN, OUTPUT);
  pinMode(ERROR_LED_PIN, OUTPUT);
  digitalWrite(DISCOVERY_LED_PIN, LOW);
  digitalWrite(ERROR_LED_PIN, LOW);

  // Initialize preferences
  preferences.begin("smart-parking", false);
  loadStoredSpots();

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  // Wait for connection with timeout
  unsigned long wifiConnectStartTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiConnectStartTime < 20000) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nFailed to connect to WiFi. Operating in offline mode.");
    systemErrorState = true;
  } else {
    Serial.println("\nConnected to WiFi");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    
    // Initialize Web Socket server
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    
    // Initialize Web Server
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(200, "text/html", mainPage);
    });
    
    server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request) {
      String jsonData = getParkingDataJson();
      request->send(200, "application/json", jsonData);
    });
    
    server.begin();
    
    // Initialize all spots with API (if they exist)
    for (int i = 0; i < connectedSpots; i++) {
      initializeSpotWithApi(i);
    }
  }
  
  Serial.println("Setup complete");
}

void loop() {
  // Handle WebSocket events
  webSocket.loop();
  
  // Check WiFi status
  checkWiFiStatus();
  
  // Handle discovery button
  handleDiscoveryButton();
  
  // Run discovery broadcasts if in discovery mode
  if (discoveryMode) {
    broadcastDiscovery();
  }
  
  // Regularly sync parking data with web clients
  if (millis() - lastSyncTime >= SYNC_INTERVAL) {
    syncParkingData();
    lastSyncTime = millis();
  }
  
  // Regularly sync data with the API
  if (WiFi.status() == WL_CONNECTED && millis() - lastApiSyncTime >= API_SYNC_INTERVAL) {
    syncWithApi();
    lastApiSyncTime = millis();
  }
  
  // Update error LED if in error state
  if (systemErrorState) {
    if (millis() - lastErrorLedToggle >= ERROR_LED_TOGGLE_INTERVAL) {
      digitalWrite(ERROR_LED_PIN, !digitalRead(ERROR_LED_PIN));
      lastErrorLedToggle = millis();
    }
  }
}

// Initialize a parking spot with the API
void initializeSpotWithApi(int index) {
  if (index < 0 || index >= connectedSpots) return;
  
  int spotId = parkingSpots[index].id;
  String endpoint = "/init/" + String(spotId);
  
  if (makeApiRequest(endpoint, "POST")) {
    Serial.println("Successfully initialized spot " + String(spotId) + " with API");
    parkingSpots[index].syncedWithApi = true;
  } else {
    Serial.println("Failed to initialize spot " + String(spotId) + " with API");
  }
}

// Update a parking spot status on the API
void updateSpotStatusOnApi(int index) {
  if (index < 0 || index >= connectedSpots) return;
  
  int spotId = parkingSpots[index].id;
  String parkingStatus = parkingSpots[index].isOccupied ? "occupied" : "open";
  bool hasSensorError = parkingSpots[index].hasSensorError;
  
  DynamicJsonDocument doc(256);
  doc["parkingStatus"] = parkingStatus;
  doc["sensorError"] = hasSensorError;
  doc["systemStatus"] = "online";
  
  String payload;
  serializeJson(doc, payload);
  
  String endpoint = "/" + String(spotId);
  
  if (makeApiRequest(endpoint, "PUT", payload)) {
    Serial.println("Successfully updated spot " + String(spotId) + " status on API");
    parkingSpots[index].syncedWithApi = true;
  } else {
    Serial.println("Failed to update spot " + String(spotId) + " status on API");
  }
}

// Send sensor data directly to the API
void sendSensorDataToApi(int index, int distance, bool hasSensorError) {
  if (index < 0 || index >= connectedSpots) return;
  
  int spotId = parkingSpots[index].id;
  
  DynamicJsonDocument doc(256);
  doc["distance"] = distance;
  doc["hasSensorError"] = hasSensorError;
  
  String payload;
  serializeJson(doc, payload);
  
  String endpoint = "/sensor/" + String(spotId);
  
  if (makeApiRequest(endpoint, "POST", payload)) {
    Serial.println("Successfully sent sensor data for spot " + String(spotId) + " to API");
    parkingSpots[index].syncedWithApi = true;
  } else {
    Serial.println("Failed to send sensor data for spot " + String(spotId) + " to API");
  }
}

// Make an HTTP request to the API
bool makeApiRequest(String endpoint, String method, String payload) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Cannot make API request: WiFi not connected");
    return false;
  }
  
  HTTPClient http;
  String url = String(API_BASE_URL) + endpoint;
  
  Serial.print("Making API request to: ");
  Serial.println(url);
  
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  
  int httpResponseCode;
  
  if (method == "GET") {
    httpResponseCode = http.GET();
  } else if (method == "POST") {
    httpResponseCode = http.POST(payload);
  } else if (method == "PUT") {
    httpResponseCode = http.PUT(payload);
  } else {
    Serial.println("Unsupported HTTP method");
    http.end();
    return false;
  }
  
  bool success = false;
  
  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    String response = http.getString();
    Serial.println("Response: " + response);
    success = (httpResponseCode == 200 || httpResponseCode == 201);
  } else {
    Serial.print("Error on API request: ");
    Serial.println(httpResponseCode);
  }
  
  http.end();
  return success;
}

// Sync all parking spot data with the API
void syncWithApi() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  Serial.println("Syncing data with API...");
  
  for (int i = 0; i < connectedSpots; i++) {
    // If the spot hasn't been initialized with the API yet, do it now
    if (!parkingSpots[i].syncedWithApi) {
      initializeSpotWithApi(i);
    }
    
    // Update the spot status on the API
    updateSpotStatusOnApi(i);
    
    // Also send the latest sensor data
    sendSensorDataToApi(i, parkingSpots[i].distance, parkingSpots[i].hasSensorError);
  }
  
  lastApiSyncTime = millis();
}

// Handle WebSocket events - IMPORTANT: Update this to send API updates when spot data changes
void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      break;
      
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[3]);
        
        // Send current parking data to newly connected client
        String jsonData = getParkingDataJson();
        webSocket.sendTXT(num, jsonData);
      }
      break;
      
    case WStype_TEXT:
      {
        Serial.printf("[%u] Received text: %s\n", num, payload);
        
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, payload);
        
        if (error) {
          Serial.println("Failed to parse JSON");
          return;
        }
        
        // Process registration messages from slaves
        if (doc.containsKey("type") && strcmp(doc["type"], "register") == 0) {
          handleRegistration(doc, num);
        }
        // Process ID update confirmations from slaves
        else if (doc.containsKey("type") && strcmp(doc["type"], "id_update_confirm") == 0) {
          handleIdUpdateConfirm(doc);
        }
        // Process distance updates from slaves
        else if (doc.containsKey("id") && doc.containsKey("distance")) {
          int id = doc["id"];
          int spotIndex = findSpotIndexById(id);
          
          if (spotIndex >= 0) {
            int distance = doc["distance"];
            bool hadSensorError = parkingSpots[spotIndex].hasSensorError;
            
            if (distance == -1 && doc.containsKey("error") && strcmp(doc["error"], "sensor_error") == 0) {
              Serial.printf("Received sensor error from Device %d\n", id);
              parkingSpots[spotIndex].hasSensorError = true;
              parkingSpots[spotIndex].lastUpdate = millis();
            } else {
              parkingSpots[spotIndex].distance = distance;
              parkingSpots[spotIndex].isOccupied = (distance < DISTANCE_THRESHOLD);
              parkingSpots[spotIndex].lastUpdate = millis();
              parkingSpots[spotIndex].hasSensorError = false;
              
              Serial.printf("Device %d reported distance: %d cm, Occupied: %s\n", 
                            id, distance, parkingSpots[spotIndex].isOccupied ? "Yes" : "No");
            }
            
            // If status has changed, update the API immediately
            if (hadSensorError != parkingSpots[spotIndex].hasSensorError || 
                parkingSpots[spotIndex].isOccupied != (distance < DISTANCE_THRESHOLD)) {
              
              sendSensorDataToApi(spotIndex, distance, parkingSpots[spotIndex].hasSensorError);
            }
            
            // Send LED command back to the slave
            sendLedStatus(spotIndex);
            
            // Save the updated spot info
            saveStoredSpots();
          } else {
            Serial.printf("Received distance from unknown device ID: %d\n", id);
          }
        }
      }
      break;
  }
}

// Other necessary functions from your original code
// ...

// Helper function to get parking data as JSON
String getParkingDataJson() {
  DynamicJsonDocument doc(1024);
  JsonArray array = doc.to<JsonArray>();
  
  for (int i = 0; i < connectedSpots; i++) {
    JsonObject spot = array.createNestedObject();
    spot["id"] = parkingSpots[i].id;
    spot["mac"] = parkingSpots[i].macAddress;
    spot["distance"] = parkingSpots[i].distance;
    spot["occupied"] = parkingSpots[i].isOccupied;
    spot["lastUpdate"] = millis() - parkingSpots[i].lastUpdate;
    spot["error"] = parkingSpots[i].hasSensorError;
  }
  
  String jsonString;
  serializeJson(array, jsonString);
  return jsonString;
}

// ... other functions from original code (handleRegistration, findSpotIndexById, etc.)