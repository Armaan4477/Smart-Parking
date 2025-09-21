#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <Preferences.h>
#include <HTTPClient.h>

const char* ssid = "JoeMama";
const char* password = "2A0R0M4AAN";

const char* API_BASE_URL = "https://smart-parking-44.vercel.app";

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
const unsigned long HEALTH_PING_INTERVAL = 20000;
unsigned long lastHealthPingTime = 0;

WebSocketsServer webSocket = WebSocketsServer(81, "", "arduino");
AsyncWebServer server(80);

const int DISTANCE_THRESHOLD = 45;
const int MAX_DISTANCE = 200;
const unsigned long SYNC_INTERVAL = 5000;
const unsigned long API_SYNC_INTERVAL = 10000;

const unsigned long DISCOVERY_DURATION = 60000;
const unsigned long DISCOVERY_INTERVAL = 5000;
unsigned long discoveryStartTime = 0;
unsigned long lastDiscoveryTime = 0;
bool discoveryMode = false;

const unsigned long OFFLINE_DETECTION_WINDOW = 10000;
const unsigned long DEVICE_OFFLINE_TIMEOUT = 30000; // 30 seconds before considering a device offline
unsigned long firstOfflineTime = 0;
int offlineCount = 0;
bool wifiDisconnectionDetected = false;

struct ParkingSpot {
  int id;
  String macAddress;
  int distance;
  bool isOccupied;
  unsigned long lastUpdate;
  bool hasSensorError = false;
  bool syncedWithApi = false;
};

const int MAX_SPOTS = 10;
ParkingSpot parkingSpots[MAX_SPOTS];
int connectedSpots = 0;
int nextAvailableId = 1;

Preferences preferences;

unsigned long lastSyncTime = 0;
unsigned long lastApiSyncTime = 0;

void initializeSpotWithApi(int spotIndex);
void updateSpotStatusOnApi(int spotIndex);
void sendSensorDataToApi(int spotIndex, int distance, bool hasSensorError);
bool makeApiRequest(String endpoint, String method, String payload = "");
void syncWithApi();
void sendHealthPing();

TaskHandle_t SlaveComTask;
TaskHandle_t SyncTask;
TaskHandle_t ApiTask;

int findParkingSpotById(int id) {
  for (int i = 0; i < connectedSpots; i++) {
    if (parkingSpots[i].id == id) {
      return i;
    }
  }
  return -1;
}

int findParkingSpotByMac(String macAddress) {
  for (int i = 0; i < connectedSpots; i++) {
    if (parkingSpots[i].macAddress == macAddress) {
      return i;
    }
  }
  return -1;
}

void loadDeviceMappings() {
  nextAvailableId = preferences.getInt("next_id", 1);
  int storedConnectedSpots = preferences.getInt("num_spots", 0);

  Serial.printf("Loaded nextAvailableId: %d, stored spots: %d\n", nextAvailableId, storedConnectedSpots);

  for (int i = 0; i < MAX_SPOTS; i++) {
    String idStr = String(i + 1);
    String macKey = "mac_" + idStr;

    String storedMac = preferences.getString(macKey.c_str(), "");

    if (storedMac.length() > 0) {
      Serial.printf("Found stored device: ID %d -> MAC %s\n", i + 1, storedMac.c_str());

      if (connectedSpots < MAX_SPOTS) {
        parkingSpots[connectedSpots].id = i + 1;
        parkingSpots[connectedSpots].macAddress = storedMac;
        parkingSpots[connectedSpots].distance = 0;
        parkingSpots[connectedSpots].isOccupied = false;
        parkingSpots[connectedSpots].lastUpdate = millis() - 20000;
        parkingSpots[connectedSpots].hasSensorError = false;
        connectedSpots++;

        Serial.printf("Restored device mapping: ID %d -> MAC %s (slot %d)\n",
                      i + 1, storedMac.c_str(), connectedSpots - 1);
      }
    }
  }

  Serial.printf("Loaded %d device mappings from EEPROM\n", connectedSpots);

  for (int i = 0; i < connectedSpots; i++) {
    if (parkingSpots[i].id >= nextAvailableId) {
      nextAvailableId = parkingSpots[i].id + 1;
    }
  }

  if (nextAvailableId != preferences.getInt("next_id", 1)) {
    preferences.putInt("next_id", nextAvailableId);
    Serial.printf("Updated nextAvailableId to %d\n", nextAvailableId);
  }
}

void saveDeviceMapping(int id, String macAddress) {
  String idStr = String(id);
  String macKey = "mac_" + idStr;

  preferences.putString(macKey.c_str(), macAddress);

  if (id >= nextAvailableId) {
    nextAvailableId = id + 1;
    preferences.putInt("next_id", nextAvailableId);
  }

  preferences.putInt("num_spots", connectedSpots);

  Serial.printf("Saved device mapping: ID %d -> MAC %s\n", id, macAddress.c_str());
}

void removeDeviceMapping(int id) {
  String idStr = String(id);
  String macKey = "mac_" + idStr;

  preferences.remove(macKey.c_str());

  preferences.putInt("num_spots", connectedSpots);

  Serial.printf("Removed device mapping for ID %d from EEPROM\n", id);

  Serial.println("Current device mappings after removal:");
  for (int i = 0; i < connectedSpots; i++) {
    Serial.printf("  Slot %d: ID %d -> MAC %s\n",
                  i, parkingSpots[i].id, parkingSpots[i].macAddress.c_str());
  }
}

int registerDevice(String macAddress, int requestedId) {
  int existingSpotIndex = findParkingSpotByMac(macAddress);

  if (existingSpotIndex >= 0) {
    int existingId = parkingSpots[existingSpotIndex].id;
    Serial.printf("Device with MAC %s already registered as ID %d\n", macAddress.c_str(), existingId);
    return existingId;
  }

  if (requestedId > 0 && requestedId <= MAX_SPOTS) {
    if (findParkingSpotById(requestedId) == -1) {
      Serial.printf("Using requested ID %d for device with MAC %s\n", requestedId, macAddress.c_str());
      return requestedId;
    }
  }

  for (int i = 1; i <= MAX_SPOTS; i++) {
    if (findParkingSpotById(i) == -1) {
      Serial.printf("Assigning next available ID %d to device with MAC %s\n", i, macAddress.c_str());
      return i;
    }
  }

  Serial.printf("ERROR: No available IDs for device with MAC %s\n", macAddress.c_str());
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

void updateSlaveDevicesStatus() {
  unsigned long currentTime = millis();
  for (int i = 0; i < connectedSpots; i++) {
    unsigned long timeSinceLastUpdate = currentTime - parkingSpots[i].lastUpdate;
    bool isOffline = timeSinceLastUpdate > DEVICE_OFFLINE_TIMEOUT;
    
    if (isOffline) {
      Serial.printf("Device %d (MAC: %s) is OFFLINE (last seen %lu ms ago)\n", 
                   parkingSpots[i].id, parkingSpots[i].macAddress.c_str(), timeSinceLastUpdate);
    }
  }
}

bool checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost! Attempting to reconnect...");

    WiFi.reconnect();

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(100);
      attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("WiFi reconnected successfully");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      return true;
    } else {
      Serial.println("Failed to reconnect to WiFi");
      return false;
    }
  }
  return true;
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
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

        if (command && strcmp(command, "update_device_id") == 0) {
          String mac = doc["mac"];
          int oldId = doc["oldId"];
          int newId = doc["newId"];

          Serial.printf("Request to update device ID: MAC %s, old ID %d -> new ID %d\n",
                        mac.c_str(), oldId, newId);

          if (findParkingSpotById(newId) != -1) {
            DynamicJsonDocument responseDoc(256);
            responseDoc["type"] = "error";
            responseDoc["message"] = "Device ID " + String(newId) + " is already assigned";

            String responseJson;
            serializeJson(responseDoc, responseJson);
            webSocket.sendTXT(num, responseJson);
            return;
          }

          int spotIndex = findParkingSpotByMac(mac);
          if (spotIndex != -1) {
            int oldDeviceId = parkingSpots[spotIndex].id;

            removeDeviceMapping(oldDeviceId);

            parkingSpots[spotIndex].id = newId;
            saveDeviceMapping(newId, mac);

            Serial.printf("Updated device ID: MAC %s, ID %d -> %d\n",
                          mac.c_str(), oldId, newId);

            for (int i = 0; i < webSocket.connectedClients(); i++) {
              IPAddress clientIP = webSocket.remoteIP(i);
              DynamicJsonDocument idUpdateDoc(256);
              idUpdateDoc["type"] = "id_update";
              idUpdateDoc["old_id"] = oldId;
              idUpdateDoc["new_id"] = newId;
              idUpdateDoc["mac"] = mac;

              String idUpdateJson;
              serializeJson(idUpdateDoc, idUpdateJson);
              webSocket.sendTXT(i, idUpdateJson);
              Serial.printf("Sent ID update notification to client %d\n", i);
            }
            
            updateSlaveDevicesStatus();
          } else {
            DynamicJsonDocument responseDoc(256);
            responseDoc["type"] = "error";
            responseDoc["message"] = "Device with MAC " + mac + " not found";

            String responseJson;
            serializeJson(responseDoc, responseJson);
            webSocket.sendTXT(num, responseJson);
          }

          return;
        }

        if (command && strcmp(command, "remove_device") == 0) {
          String mac = doc["mac"];
          int id = doc["id"];

          Serial.printf("Request to remove device: MAC %s, ID %d\n", mac.c_str(), id);

          int spotIndex = findParkingSpotByMac(mac);
          if (spotIndex != -1) {
            removeDeviceMapping(id);

            for (int i = spotIndex; i < connectedSpots - 1; i++) {
              parkingSpots[i] = parkingSpots[i + 1];
            }
            connectedSpots--;

            Serial.printf("Removed device: MAC %s, ID %d\n", mac.c_str(), id);

            updateSlaveDevicesStatus();
          } else {
            DynamicJsonDocument responseDoc(256);
            responseDoc["type"] = "error";
            responseDoc["message"] = "Device with MAC " + mac + " not found";

            String responseJson;
            serializeJson(responseDoc, responseJson);
            webSocket.sendTXT(num, responseJson);
          }

          return;
        }

        if (doc.containsKey("type") && strcmp(doc["type"], "id_update_confirm") == 0) {
          String macAddress = doc["mac"];
          int oldId = doc["old_id"];
          int newId = doc["new_id"];

          Serial.printf("Received ID update confirmation from device MAC: %s (ID changed from %d to %d)\n",
                        macAddress.c_str(), oldId, newId);

          DynamicJsonDocument notifyDoc(256);
          notifyDoc["type"] = "notification";
          notifyDoc["message"] = "Device " + macAddress + " confirmed ID change from " + String(oldId) + " to " + String(newId);
          notifyDoc["notificationType"] = "success";

          String notifyJson;
          serializeJson(notifyDoc, notifyJson);

          webSocket.broadcastTXT(notifyJson);

          return;
        }

        if (doc.containsKey("type") && strcmp(doc["type"], "register") == 0) {
          String macAddress = doc["mac"].as<String>();
          int currentId = doc["currentId"] | -1;

          Serial.printf("Registration request from device with MAC: %s, currentId: %d\n",
                        macAddress.c_str(), currentId);

          int spotIndex = findParkingSpotByMac(macAddress);
          int assignedId = -1;

          bool wasOffline = false;
          if (spotIndex != -1) {
            unsigned long timeSinceLastUpdate = millis() - parkingSpots[spotIndex].lastUpdate;
            if (timeSinceLastUpdate > 10000) {
              wasOffline = true;
            }
          }

          if (spotIndex != -1) {
            assignedId = parkingSpots[spotIndex].id;
            Serial.printf("Device with MAC %s already registered with ID %d\n",
                          macAddress.c_str(), assignedId);

            parkingSpots[spotIndex].lastUpdate = millis();

            if (currentId > 0 && currentId != assignedId) {
              Serial.printf("WARNING: Device with MAC %s requested ID %d but is registered as ID %d\n",
                            macAddress.c_str(), currentId, assignedId);
            }
          } else if (connectedSpots < MAX_SPOTS) {

            bool foundInEEPROM = false;
            int storedId = -1;

            for (int i = 1; i <= MAX_SPOTS; i++) {
              String macKey = "mac_" + String(i);
              String storedMac = preferences.getString(macKey.c_str(), "");

              if (storedMac == macAddress) {
                foundInEEPROM = true;
                storedId = i;
                break;
              }
            }

            if (foundInEEPROM) {
              if (findParkingSpotById(storedId) == -1) {
                assignedId = storedId;
                Serial.printf("Re-using stored ID %d for returning device with MAC %s\n",
                              assignedId, macAddress.c_str());
              } else {
                assignedId = registerDevice(macAddress, currentId);
                Serial.printf("Device with MAC %s had stored ID %d but that's in use now, assigning new ID %d\n",
                              macAddress.c_str(), storedId, assignedId);
              }
            } else {
              assignedId = registerDevice(macAddress, currentId);
              Serial.printf("Registering new device with MAC %s as ID %d\n",
                            macAddress.c_str(), assignedId);
            }

            if (assignedId > 0) {
              spotIndex = connectedSpots;
              parkingSpots[spotIndex].id = assignedId;
              parkingSpots[spotIndex].macAddress = macAddress;
              parkingSpots[spotIndex].distance = 0;
              parkingSpots[spotIndex].isOccupied = false;
              parkingSpots[spotIndex].hasSensorError = false;
              parkingSpots[spotIndex].lastUpdate = millis();
              connectedSpots++;

              saveDeviceMapping(assignedId, macAddress);

              Serial.printf("New device registered: MAC %s -> ID %d\n",
                            macAddress.c_str(), assignedId);
            } else {
              Serial.println("Failed to assign ID: No IDs available");
            }
          } else {
            Serial.println("Failed to register device: Maximum number of spots reached");
          }

          DynamicJsonDocument response(256);
          response["type"] = "id_assigned";
          response["assigned_id"] = assignedId;
          response["mac"] = macAddress;

          String jsonResponse;
          serializeJson(response, jsonResponse);
          webSocket.sendTXT(num, jsonResponse);

          if (wasOffline && assignedId > 0) {
            DynamicJsonDocument notifyDoc(256);
            notifyDoc["type"] = "notification";
            notifyDoc["message"] = "Spot " + String(assignedId) + " is back online";
            notifyDoc["notificationType"] = "success";

            String notifyJson;
            serializeJson(notifyDoc, notifyJson);
            webSocket.broadcastTXT(notifyJson);
          }

          return;
        }

        if (doc.containsKey("id") && doc.containsKey("distance")) {
          int id = doc["id"];
          int distance = doc["distance"];
          String macAddress = doc["mac"].as<String>();
          bool hasSensorError = false;

          if (doc.containsKey("error") && strcmp(doc["error"], "sensor_error") == 0) {
            bool prevErrorState = false;
            int spotIndex = findParkingSpotById(id);
            if (spotIndex != -1) {
              prevErrorState = parkingSpots[spotIndex].hasSensorError;
            }

            hasSensorError = true;
            Serial.printf("Sensor error reported by device ID %d (MAC: %s)\n", id, macAddress.c_str());
            
            systemErrorState = true;

            if (spotIndex != -1 && !prevErrorState) {
              DynamicJsonDocument notifyDoc(256);
              notifyDoc["type"] = "notification";
              notifyDoc["message"] = "Sensor error detected on spot " + String(id);
              notifyDoc["notificationType"] = "error";

              String notifyJson;
              serializeJson(notifyDoc, notifyJson);
              webSocket.broadcastTXT(notifyJson);
            }
          }

          int spotIndex = findParkingSpotById(id);
          if (spotIndex == -1 && connectedSpots < MAX_SPOTS) {
            Serial.printf("Warning: Received update for unregistered ID %d\n", id);
            return;
          }

          if (spotIndex != -1) {
            bool wasOccupied = parkingSpots[spotIndex].isOccupied;
            bool hadSensorError = parkingSpots[spotIndex].hasSensorError;

            if (macAddress.length() > 0) {
              parkingSpots[spotIndex].macAddress = macAddress;
            }

            parkingSpots[spotIndex].distance = distance;

            if (!hasSensorError) {
              parkingSpots[spotIndex].isOccupied = (distance <= DISTANCE_THRESHOLD && distance > 0);
            }

            parkingSpots[spotIndex].lastUpdate = millis();

            parkingSpots[spotIndex].hasSensorError = hasSensorError;
            
            if (wasOccupied != parkingSpots[spotIndex].isOccupied || 
                hadSensorError != parkingSpots[spotIndex].hasSensorError) {
                  
              if (WiFi.status() == WL_CONNECTED) {
                sendSensorDataToApi(spotIndex, distance, hasSensorError);
              }
            }

            if (hadSensorError && !hasSensorError) {
              DynamicJsonDocument notifyDoc(256);
              notifyDoc["type"] = "notification";
              notifyDoc["message"] = "Sensor on spot " + String(id) + " is working again";
              notifyDoc["notificationType"] = "success";

              String notifyJson;
              serializeJson(notifyDoc, notifyJson);
              webSocket.broadcastTXT(notifyJson);
              
              bool anyErrorsLeft = false;
              for (int i = 0; i < connectedSpots; i++) {
                if (parkingSpots[i].hasSensorError) {
                  anyErrorsLeft = true;
                  break;
                }
              }
              
              if (!anyErrorsLeft && !wifiDisconnectionDetected) {
                systemErrorState = false;
              }
            }

            DynamicJsonDocument response(128);
            response["id"] = id;
            response["led"] = parkingSpots[spotIndex].isOccupied ? "red" : "green";

            String jsonResponse;
            serializeJson(response, jsonResponse);
            webSocket.sendTXT(num, jsonResponse);

            Serial.printf("Updated spot %d (MAC: %s): distance=%d, occupied=%s\n",
                          id, parkingSpots[spotIndex].macAddress.c_str(),
                          distance, parkingSpots[spotIndex].isOccupied ? "yes" : "no");

            if (wasOccupied != parkingSpots[spotIndex].isOccupied) {
              Serial.println("Spot status changed, updating device status");
              updateSlaveDevicesStatus();
            }
          }
        }
      }
      break;
  }
}

void slaveComTaskFunction(void* parameter) {
  for (;;) {
    webSocket.loop();
    
    checkDiscoveryButton();
    bool buttonState = digitalRead(DISCOVERY_BUTTON_PIN);
    if (buttonState == LOW) {
      physicalOverrideActive = true;
      discoveryMode = true;
    } else {
      physicalOverrideActive = false;
    }
    
    unsigned long currentTime = millis();
    
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
    
    systemErrorState = checkForSystemErrors();
    if (systemErrorState) {
      if (currentTime - lastErrorLedToggle >= ERROR_LED_TOGGLE_INTERVAL) {
        static bool errorLedState = false;
        errorLedState = !errorLedState;
        digitalWrite(ERROR_LED_PIN, errorLedState ? HIGH : LOW);
        lastErrorLedToggle = currentTime;
      }
    } else {
      digitalWrite(ERROR_LED_PIN, LOW);
    }
    
    delay(1);
  }
}

void apiTaskFunction(void* parameter) {
  for (;;) {
    unsigned long currentTime = millis();
    
    if (WiFi.status() == WL_CONNECTED && currentTime - lastApiSyncTime >= API_SYNC_INTERVAL) {
      syncWithApi();
      lastApiSyncTime = currentTime;
    }
    
    if (WiFi.status() == WL_CONNECTED && currentTime - lastHealthPingTime >= HEALTH_PING_INTERVAL) {
      sendHealthPing();
    }
    
    static unsigned long lastWiFiCheck = 0;
    if (wifiDisconnectionDetected && currentTime - lastWiFiCheck >= 30000) {
      Serial.println("Periodic WiFi connection check...");
      if (checkWiFiConnection()) {
        wifiDisconnectionDetected = false;
        Serial.println("WiFi connection restored");
        
        bool anyErrorsLeft = false;
        for (int i = 0; i < connectedSpots; i++) {
          if (parkingSpots[i].hasSensorError) {
            anyErrorsLeft = true;
            break;
          }
        }
        
        if (!anyErrorsLeft) {
          systemErrorState = false;
        }
      }
      lastWiFiCheck = currentTime;
    }
    
    delay(100);
  }
}

void syncTaskFunction(void* parameter) {
  unsigned long lastSyncTime = 0;
  unsigned long lastDiscoveryTime = 0;
  unsigned long lastConnectivityCheck = 0;
  int previousActiveSpots = 0;

  for (;;) {
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
        
        Serial.println("Discovery mode ended due to timeout");
      }
    }

    if (currentTime - lastSyncTime >= SYNC_INTERVAL) {
      Serial.println("Syncing parking status...");

      int activeSpots = 0;
      int offlineSpots = 0;
      int recentlyOfflineSpots = 0;

      for (int i = 0; i < connectedSpots; i++) {
        unsigned long timeSinceUpdate = currentTime - parkingSpots[i].lastUpdate;
        bool wasOffline = timeSinceUpdate > 10000;
        bool isNowOffline = timeSinceUpdate > 10000;

        static unsigned long lastOfflineCheckTime = 0;
        static bool spotOfflineStatus[MAX_SPOTS] = { false };

        if (currentTime - lastOfflineCheckTime >= 5000) {
          if (!spotOfflineStatus[i] && isNowOffline) {
            DynamicJsonDocument notifyDoc(256);
            notifyDoc["type"] = "notification";
            notifyDoc["message"] = "Spot " + String(parkingSpots[i].id) + " went offline";
            notifyDoc["notificationType"] = "warning";

            String notifyJson;
            serializeJson(notifyDoc, notifyJson);
            webSocket.broadcastTXT(notifyJson);

            spotOfflineStatus[i] = true;
          }

          if (i == connectedSpots - 1) {
            lastOfflineCheckTime = currentTime;
          }
        }

        if (timeSinceUpdate <= 10000) {
          activeSpots++;
          spotOfflineStatus[i] = false;
        } else {
          offlineSpots++;

          if (parkingSpots[i].lastUpdate > 0 && parkingSpots[i].lastUpdate > currentTime - OFFLINE_DETECTION_WINDOW) {
            recentlyOfflineSpots++;

            if (firstOfflineTime == 0) {
              firstOfflineTime = currentTime;
              offlineCount = 1;
            } else {
              offlineCount++;
            }
          }
        }
      }

      Serial.printf("Active spots: %d, Offline spots: %d, Recently offline: %d\n",
                    activeSpots, offlineSpots, recentlyOfflineSpots);

      if (firstOfflineTime > 0 && currentTime - firstOfflineTime > OFFLINE_DETECTION_WINDOW) {
        Serial.println("Resetting offline detection window");
        firstOfflineTime = 0;
        offlineCount = 0;
      }

      int previousTotal = previousActiveSpots;
      if (previousTotal > 0 && offlineCount > 0) {
        float offlinePercentage = (float)offlineCount / previousTotal;

        Serial.printf("Offline percentage: %.1f%% (%d out of previous %d)\n",
                      offlinePercentage * 100, offlineCount, previousTotal);

        if (offlinePercentage >= 0.5 && !discoveryMode) {
          Serial.println("ALERT: Multiple slaves went offline! Checking WiFi connection...");

          bool wifiOK = checkWiFiConnection();

          if (!wifiOK) {
            Serial.println("WiFi connection issue detected! Starting discovery mode...");
            wifiDisconnectionDetected = true;
          } else {
            Serial.println("WiFi connection is OK, but slaves are offline. Starting discovery mode...");
          }

          discoveryMode = true;
          discoveryStartTime = currentTime;
          lastDiscoveryTime = 0;
          digitalWrite(DISCOVERY_LED_PIN, HIGH);

          DynamicJsonDocument responseDoc(256);
          responseDoc["type"] = "discovery_status";
          responseDoc["active"] = true;
          responseDoc["physical_override"] = false;
          // No WebUI anymore, just log the state change
          String message = wifiOK ? "Discovery mode activated due to multiple slaves offline" : "Discovery mode activated due to WiFi connection issue";
          Serial.println(message);

          firstOfflineTime = 0;
          offlineCount = 0;
        }
      }

      previousActiveSpots = activeSpots;

      updateSlaveDevicesStatus();
      lastSyncTime = currentTime;
    }

    delay(100);
  }
}

void WiFiEventHandler(WiFiEvent_t event) {
  switch (event) {
    case WIFI_EVENT_STA_DISCONNECTED:
      Serial.println("WiFi lost connection");
      wifiDisconnectionDetected = true;
      break;
    case IP_EVENT_STA_GOT_IP:
      Serial.println("WiFi connected with IP: ");
      Serial.println(WiFi.localIP());
      wifiDisconnectionDetected = false;
      break;
    default:
      break;
  }
}

void printStoredDeviceMappings() {
  Serial.println("\n--- STORED DEVICE MAPPINGS ---");
  Serial.printf("Next available ID: %d\n", preferences.getInt("next_id", 1));
  Serial.printf("Stored spots count: %d\n", preferences.getInt("num_spots", 0));

  int foundMappings = 0;
  for (int i = 1; i <= MAX_SPOTS; i++) {
    String macKey = "mac_" + String(i);
    String storedMac = preferences.getString(macKey.c_str(), "");

    if (storedMac.length() > 0) {
      Serial.printf("ID %d -> MAC %s\n", i, storedMac.c_str());
      foundMappings++;
    }
  }

  Serial.printf("Found %d stored mappings in EEPROM\n", foundMappings);
  Serial.println("----------------------------\n");
}

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Master - Smart Parking System");

  pinMode(DISCOVERY_BUTTON_PIN, INPUT_PULLUP);
  pinMode(DISCOVERY_LED_PIN, OUTPUT);
  pinMode(ERROR_LED_PIN, OUTPUT);
  digitalWrite(DISCOVERY_LED_PIN, LOW);
  digitalWrite(ERROR_LED_PIN, LOW);

  preferences.begin("smartpark", false);

  Serial.println("Initializing device mapping storage in EEPROM...");
  printStoredDeviceMappings();

  WiFi.onEvent(WiFiEventHandler);

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

  server.begin();
  Serial.println("HTTP server started");

  for (int i = 0; i < MAX_SPOTS; i++) {
    parkingSpots[i].id = -1;
    parkingSpots[i].macAddress = "";
    parkingSpots[i].distance = 0;
    parkingSpots[i].isOccupied = false;
    parkingSpots[i].hasSensorError = false;
    parkingSpots[i].lastUpdate = 0;
  }

  loadDeviceMappings();
  
  if (WiFi.status() == WL_CONNECTED) {
    for (int i = 0; i < connectedSpots; i++) {
      initializeSpotWithApi(i);
    }
  }

  xTaskCreatePinnedToCore(
    slaveComTaskFunction,
    "SlaveComTask",
    10000,
    NULL,
    1,
    &SlaveComTask,
    0);

  xTaskCreatePinnedToCore(
    syncTaskFunction,
    "SyncTask",
    10000,
    NULL,
    1,
    &SyncTask,
    0);
    
  xTaskCreatePinnedToCore(
    apiTaskFunction,
    "ApiTask",
    10000,
    NULL,
    1,
    &ApiTask,
    1);
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

    Serial.println("Physical discovery override activated!");
  }

  if (buttonState == HIGH && lastButtonState == LOW) {
    Serial.println("Discovery button released - physical override deactivated!");
    physicalOverrideActive = false;

    discoveryMode = false;
    digitalWrite(DISCOVERY_LED_PIN, LOW);

    DynamicJsonDocument responseDoc(128);
    responseDoc["type"] = "discovery_status";
    responseDoc["active"] = false;
    Serial.println("Physical discovery override deactivated!");
  }

  lastButtonState = buttonState;
}

bool checkForSystemErrors() {
  bool hasSensorErrors = false;
  bool hasSlaveConnectionErrors = false;
  bool hasMasterConnectionErrors = false;
  unsigned long currentTime = millis();
  
  for (int i = 0; i < connectedSpots; i++) {
    if (parkingSpots[i].hasSensorError) {
      hasSensorErrors = true;
      break;
    }
    
    if ((currentTime - parkingSpots[i].lastUpdate) > 15000) {
      hasSlaveConnectionErrors = true;
    }
  }
  
  if (wifiDisconnectionDetected) {
    hasMasterConnectionErrors = true;
  }

  return hasSensorErrors || hasMasterConnectionErrors;
}

void loop() {

  delay(100);
}

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

void updateSpotStatusOnApi(int index) {
  if (index < 0 || index >= connectedSpots) return;
  
  int spotId = parkingSpots[index].id;
  String parkingStatus = parkingSpots[index].isOccupied ? "occupied" : "open";
  bool hasSensorError = parkingSpots[index].hasSensorError;
  
  unsigned long currentTime = millis();
  unsigned long timeSinceLastUpdate = currentTime - parkingSpots[index].lastUpdate;
  bool isOffline = timeSinceLastUpdate > DEVICE_OFFLINE_TIMEOUT;
  
  String systemStatus = isOffline ? "offline" : "online";
  
  DynamicJsonDocument doc(256);
  doc["parkingStatus"] = parkingStatus;
  doc["sensorError"] = hasSensorError;
  doc["systemStatus"] = systemStatus;
  
  doc["lastUpdateMs"] = parkingSpots[index].lastUpdate;
  doc["currentTimeMs"] = currentTime;
  doc["timeSinceUpdateMs"] = timeSinceLastUpdate;
  
  String payload;
  serializeJson(doc, payload);
  
  String endpoint = "/" + String(spotId);
  
  if (makeApiRequest(endpoint, "PUT", payload)) {
    Serial.println("Successfully updated spot " + String(spotId) + " status on API");
    Serial.printf("Device %d is %s (last update: %lu ms ago)\n", 
                  spotId, isOffline ? "OFFLINE" : "ONLINE", timeSinceLastUpdate);
    parkingSpots[index].syncedWithApi = true;
  } else {
    Serial.println("Failed to update spot " + String(spotId) + " status on API");
  }
}

void sendSensorDataToApi(int index, int distance, bool hasSensorError) {
  if (index < 0 || index >= connectedSpots) return;
  
  int spotId = parkingSpots[index].id;
  
  unsigned long currentTime = millis();
  unsigned long timeSinceLastUpdate = currentTime - parkingSpots[index].lastUpdate;
  bool isOffline = timeSinceLastUpdate > DEVICE_OFFLINE_TIMEOUT;
  
  DynamicJsonDocument doc(256);
  doc["distance"] = distance;
  doc["hasSensorError"] = hasSensorError;
  doc["isOffline"] = isOffline;
  doc["lastUpdateMs"] = parkingSpots[index].lastUpdate;
  doc["timeSinceUpdateMs"] = timeSinceLastUpdate;
  
  String payload;
  serializeJson(doc, payload);
  
  String endpoint = "/sensor/" + String(spotId);
  
  if (makeApiRequest(endpoint, "POST", payload)) {
    Serial.println("Successfully sent sensor data for spot " + String(spotId) + " to API");
    Serial.printf("Device %d is %s (last update: %lu ms ago)\n", 
                  spotId, isOffline ? "OFFLINE" : "ONLINE", timeSinceLastUpdate);
    parkingSpots[index].syncedWithApi = true;
  } else {
    Serial.println("Failed to send sensor data for spot " + String(spotId) + " to API");
  }
}

bool makeApiRequest(String endpoint, String method, String payload) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Cannot make API request: WiFi not connected");
    return false;
  }
  
  HTTPClient http;
  String url = String(API_BASE_URL) + "/api/parking" + endpoint;
  
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

void sendHealthPing() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  Serial.println("Sending health ping to API...");
  
  StaticJsonDocument<256> jsonDoc;
  jsonDoc["deviceId"] = "master";
  jsonDoc["systemStatus"] = "online";
  jsonDoc["wifiConnected"] = (WiFi.status() == WL_CONNECTED);
  jsonDoc["slaveDevices"] = connectedSpots;
  
  String payload;
  serializeJson(jsonDoc, payload);
  
  if (makeApiRequest("/health", "POST", payload)) {
    Serial.println("Health ping sent successfully");
  } else {
    Serial.println("Failed to send health ping");
  }
  
  lastHealthPingTime = millis();
}

void syncWithApi() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  Serial.println("Syncing data with API...");
  
  sendHealthPing();
  
  for (int i = 0; i < connectedSpots; i++) {
    if (!parkingSpots[i].syncedWithApi) {
      initializeSpotWithApi(i);
    }
    
    updateSpotStatusOnApi(i);
    
    sendSensorDataToApi(i, parkingSpots[i].distance, parkingSpots[i].hasSensorError);
  }
  
  lastApiSyncTime = millis();
}
