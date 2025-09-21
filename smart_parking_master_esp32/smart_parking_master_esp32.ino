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

const char mainPage[] PROGMEM = R"html(
<!DOCTYPE html>
<html>
<head>
  <title>Smart Parking System</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    :root {
      --primary: #3498db;
      --primary-dark: #2980b9;
      --secondary: #2c3e50;
      --success: #2ecc71;
      --danger: #e74c3c;
      --warning: #f39c12;
      --light: #ecf0f1;
      --dark: #34495e;
      --shadow: 0 4px 6px rgba(0,0,0,0.1);
      --transition: all 0.3s ease;
    }
    
    * {
      margin: 0;
      padding: 0;
      box-sizing: border-box;
    }
    
    body {
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      background: linear-gradient(135deg, #f5f7fa 0%, #c3cfe2 100%);
      min-height: 100vh;
      padding: 20px;
      color: var(--secondary);
    }
    
    .container {
      max-width: 1000px;
      margin: 0 auto;
    }
    
    .header {
      background-color: white;
      padding: 20px 30px;
      border-radius: 12px;
      box-shadow: var(--shadow);
      margin-bottom: 20px;
      display: flex;
      align-items: center;
      justify-content: space-between;
    }
    
    .logo {
      display: flex;
      align-items: center;
    }
    
    h1 {
      font-size: 1.8rem;
      font-weight: 600;
      color: var(--secondary);
      margin: 0;
    }
    
    .card {
      background-color: white;
      border-radius: 12px;
      box-shadow: var(--shadow);
      overflow: hidden;
      transition: var(--transition);
      margin-bottom: 20px;
    }
    
    .card:hover {
      transform: translateY(-5px);
      box-shadow: 0 8px 15px rgba(0,0,0,0.1);
    }
    
    .card-header {
      padding: 15px 20px;
      border-bottom: 1px solid var(--light);
      display: flex;
      align-items: center;
      justify-content: space-between;
    }
    
    .card-header h2 {
      font-size: 1.2rem;
      font-weight: 600;
      margin: 0;
      display: flex;
      align-items: center;
    }
    
    .card-body {
      padding: 20px;
    }
    
    .dashboard-stats {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
      gap: 20px;
      margin-bottom: 20px;
    }
    
    .stat-card {
      border-radius: 10px;
      padding: 20px;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      color: white;
      transition: var(--transition);
      text-align: center;
    }
    
    .stat-card:hover {
      transform: scale(1.05);
    }
    
    .stat-card .stat-value {
      font-size: 1.8rem;
      font-weight: bold;
      margin-bottom: 5px;
    }
    
    .stat-card .stat-label {
      font-size: 0.9rem;
      opacity: 0.9;
    }
    
    .total-stat {
      background: linear-gradient(135deg, #3498db 0%, #2980b9 100%);
    }
    
    .available-stat {
      background: linear-gradient(135deg, #2ecc71 0%, #27ae60 100%);
    }
    
    .occupied-stat {
      background: linear-gradient(135deg, #e74c3c 0%, #c0392b 100%);
    }
    
    .error-stat {
      background: linear-gradient(135deg, #9b59b6 0%, #8e44ad 100%);
    }
    
    .button {
      border: none;
      border-radius: 6px;
      padding: 10px 20px;
      font-weight: 500;
      cursor: pointer;
      display: flex;
      align-items: center;
      justify-content: center;
      transition: var(--transition);
      font-size: 0.9rem;
    }
    
    .button-primary {
      background-color: var(--primary);
      color: white;
    }
    
    .button-primary:hover {
      background-color: var(--primary-dark);
      transform: translateY(-2px);
    }
    
    .button-danger {
      background-color: var(--danger);
      color: white;
    }
    
    .button-danger:hover {
      background-color: #c0392b;
      transform: translateY(-2px);
    }
    
    .button-secondary {
      background-color: var(--secondary);
      color: white;
    }
    
    .button-secondary:hover {
      background-color: var(--dark);
      transform: translateY(-2px);
    }
    
    .button:disabled {
      opacity: 0.6;
      cursor: not-allowed;
      transform: none;
    }
    
    .discovery-controls {
      display: flex;
      gap: 10px;
      margin-top: 15px;
      align-items: center;
    }
    
    .discovery-status {
      display: none;
      font-weight: 500;
      padding: 5px 12px;
      border-radius: 20px;
      font-size: 0.85rem;
      animation: pulse 1.5s infinite;
      margin-left: 10px;
    }
    
    @keyframes pulse {
      0% { opacity: 0.7; }
      50% { opacity: 1; }
      100% { opacity: 0.7; }
    }
    
    .status-active {
      background-color: rgba(46, 204, 113, 0.2);
      color: #27ae60;
      border: 1px solid rgba(46, 204, 113, 0.3);
    }
    
    .status-override {
      background-color: rgba(52, 152, 219, 0.2);
      color: #2980b9;
      border: 1px solid rgba(52, 152, 219, 0.3);
    }
    
    .status-inactive {
      background-color: rgba(231, 76, 60, 0.2);
      color: #c0392b;
      border: 1px solid rgba(231, 76, 60, 0.3);
      animation: none;
    }
    
    .parking-grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(200px, 1fr));
      gap: 15px;
    }
    
    .parking-spot {
      border-radius: 10px;
      padding: 0;
      overflow: hidden;
      box-shadow: 0 2px 8px rgba(0,0,0,0.08);
      transition: all 0.3s ease;
    }
    
    .parking-spot:hover {
      transform: translateY(-3px);
      box-shadow: 0 5px 15px rgba(0,0,0,0.1);
    }
    
    .spot-header {
      padding: 12px;
      font-weight: 600;
      text-align: center;
      color: white;
      display: flex;
      align-items: center;
      justify-content: center;
    }
    
    .spot-available .spot-header {
      background: linear-gradient(135deg, #2ecc71, #27ae60);
    }
    
    .spot-occupied .spot-header {
      background: linear-gradient(135deg, #e74c3c, #c0392b);
    }
    
    .spot-offline .spot-header {
      background: linear-gradient(135deg, #f39c12, #d35400);
    }
    
    .spot-error .spot-header {
      background: linear-gradient(135deg, #9b59b6, #8e44ad);
    }
    
    .spot-body {
      background-color: white;
      padding: 12px;
      text-align: center;
    }
    
    .spot-id {
      font-size: 1.4rem;
      font-weight: bold;
      margin-bottom: 5px;
    }
    
    .spot-status {
      font-size: 0.85rem;
      color: #7f8c8d;
      margin-bottom: 0;
    }
    
    .device-list {
      display: flex;
      flex-direction: column;
      gap: 10px;
      margin: 15px 0;
    }
    
    .device-item {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: 12px 15px;
      background-color: white;
      border-radius: 8px;
      box-shadow: 0 2px 5px rgba(0,0,0,0.05);
      transition: var(--transition);
      border-left: 4px solid var(--primary);
    }
    
    .device-item:hover {
      box-shadow: 0 5px 15px rgba(0,0,0,0.08);
    }
    
    .device-offline {
      border-left: 4px solid var(--warning);
      opacity: 0.8;
    }
    
    .device-info {
      display: flex;
      align-items: center;
    }
    
    .device-id {
      font-weight: 600;
      margin-right: 10px;
      background-color: var(--light);
      color: var(--secondary);
      border-radius: 4px;
      padding: 3px 8px;
    }
    
    .device-mac {
      color: #7f8c8d;
      font-size: 0.9rem;
    }
    
    .device-status {
      display: flex;
      align-items: center;
      margin-left: 12px;
      font-size: 0.8rem;
      font-weight: 500;
    }
    
    .status-indicator {
      width: 8px;
      height: 8px;
      border-radius: 50%;
      margin-right: 5px;
    }
    
    .status-online .status-indicator {
      background-color: var(--success);
      box-shadow: 0 0 0 2px rgba(46, 204, 113, 0.2);
    }
    
    .status-offline .status-indicator {
      background-color: var(--warning);
      box-shadow: 0 0 0 2px rgba(243, 156, 18, 0.2);
    }
    
    .status-error .status-indicator {
      background-color: var(--primary);
      box-shadow: 0 0 0 2px rgba(155, 89, 182, 0.2);
    }
    
    .device-buttons button {
      background-color: var(--light);
      color: var(--secondary);
      border: none;
      border-radius: 4px;
      padding: 6px 12px;
      cursor: pointer;
      font-size: 0.85rem;
      transition: var(--transition);
    }
    
    .device-buttons button:hover {
      background-color: var(--secondary);
      color: white;
    }
    
    .device-editor {
      background-color: white;
      border-radius: 8px;
      padding: 20px;
      box-shadow: 0 3px 10px rgba(0,0,0,0.1);
      margin-top: 20px;
      display: none;
      animation: slideDown 0.3s ease;
    }
    
    @keyframes slideDown {
      from { opacity: 0; transform: translateY(-10px); }
      to { opacity: 1; transform: translateY(0); }
    }
    
    .editor-header {
      display: flex;
      align-items: center;
      margin-bottom: 15px;
    }
    
    .editor-title {
      font-size: 1.1rem;
      font-weight: 600;
      margin: 0;
    }
    
    .editor-mac {
      font-size: 0.9rem;
      color: #7f8c8d;
      margin: 10px 0;
    }
    
    .editor-form {
      margin: 15px 0;
    }
    
    .form-group {
      margin-bottom: 15px;
    }
    
    .form-group label {
      display: block;
      margin-bottom: 5px;
      font-weight: 500;
      font-size: 0.9rem;
    }
    
    .form-control {
      width: 100%;
      padding: 8px 12px;
      border: 1px solid #ddd;
      border-radius: 4px;
      font-size: 0.9rem;
    }
    
    input[type="number"].form-control {
      max-width: 120px;
    }
    
    .editor-actions {
      display: flex;
      gap: 10px;
    }

    .footer {
      text-align: center;
      margin-top: 30px;
      padding: 20px;
      color: #7f8c8d;
      font-size: 0.9rem;
    }
    
    @media (max-width: 768px) {
      .dashboard-stats {
        grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
      }
      
      .header {
        flex-direction: column;
        align-items: flex-start;
      }
      
      .discovery-controls {
        margin-top: 15px;
        width: 100%;
      }
    }

    .fade-in {
      animation: fadeIn 0.5s ease-in-out;
    }
    
    @keyframes fadeIn {
      from { opacity: 0; }
      to { opacity: 1; }
    }
    
    .bounce {
      animation: bounce 0.5s;
    }
    
    @keyframes bounce {
      0%, 100% { transform: translateY(0); }
      50% { transform: translateY(-10px); }
    }

    .section-header {
      margin-bottom: 15px;
      display: flex;
      align-items: center;
    }
    
    .section-title {
      font-size: 1.1rem;
      font-weight: 600;
      margin: 0;
      color: var(--secondary);
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <div class="logo">
        <h1>Smart Parking System</h1>
      </div>
    </div>
    
    <!-- Dashboard Stats -->
    <div class="dashboard-stats">
      <div class="stat-card total-stat">
        <div class="stat-value" id="totalSpots">0</div>
        <div class="stat-label">Total Spots</div>
      </div>
      
      <div class="stat-card available-stat">
        <div class="stat-value" id="availableSpots">0</div>
        <div class="stat-label">Available</div>
      </div>
      
      <div class="stat-card occupied-stat">
        <div class="stat-value" id="occupiedSpots">0</div>
        <div class="stat-label">Occupied</div>
      </div>
      
      <div class="stat-card error-stat">
        <div class="stat-value" id="errorSpots">0</div>
        <div class="stat-label">Sensor Errors</div>
      </div>
    </div>
    
    <!-- Discovery Section -->
    <div class="card">
      <div class="card-header">
        <h2>Device Discovery</h2>
      </div>
      <div class="card-body">
        <div class="discovery-controls">
          <button id="startDiscoveryBtn" class="button button-primary">
            Start Discovery
          </button>
          <button id="stopDiscoveryBtn" class="button button-danger">
            Stop Discovery
          </button>
          <span id="discoveryStatus" class="discovery-status">Discovery mode active...</span>
        </div>
      </div>
    </div>
    
    <!-- Device Management -->
    <div class="card">
      <div class="card-header">
        <h2>Device Management</h2>
      </div>
      <div class="card-body">
        <div class="section-header">
          <h3 class="section-title">Connected Devices</h3>
        </div>
        
        <div id="deviceList" class="device-list">
        </div>
        
        <div id="deviceEditor" class="device-editor">
          <div class="editor-header">
            <h3 class="editor-title">Edit Device <span id="editDeviceId"></span></h3>
          </div>
          
          <p class="editor-mac">MAC Address: <span id="editDeviceMac"></span></p>
          
          <div class="editor-form">
            <div class="form-group">
              <label for="newDeviceId">Assign new ID:</label>
              <input type="number" id="newDeviceId" class="form-control" min="1" max="10">
            </div>
          </div>
          
          <div class="editor-actions">
            <button id="saveDeviceBtn" class="button button-primary">
              Save Changes
            </button>
            <button id="removeDeviceBtn" class="button button-danger">
              Remove Device
            </button>
            <button id="cancelEditBtn" class="button button-secondary">
              Cancel
            </button>
          </div>
        </div>
      </div>
    </div>
    
    <div class="card">
      <div class="card-header">
        <h2>Parking Status</h2>
      </div>
      <div class="card-body">
        <div id="statusGrid" class="parking-grid">
        </div>
      </div>
    </div>
    
    <div class="footer">
      &copy; 2025 Smart Parking System | All Rights Reserved
    </div>
  </div>

  <script>
    var socket;
    var lastParkingState = {};
    
    function showNotification(message, type = 'success') {
      let notificationContainer = document.getElementById('notificationContainer');
      if (!notificationContainer) {
        notificationContainer = document.createElement('div');
        notificationContainer.id = 'notificationContainer';
        notificationContainer.style = `
          position: fixed;
          top: 20px;
          right: 20px;
          max-width: 300px;
          z-index: 1000;
        `;
        document.body.appendChild(notificationContainer);
      }
      
      const notification = document.createElement('div');
      notification.style = `
        background-color: ${type === 'success' ? '#2ecc71' : type === 'error' ? '#e74c3c' : '#f39c12'};
        color: white;
        padding: 12px 15px;
        margin-bottom: 10px;
        border-radius: 8px;
        box-shadow: 0 3px 10px rgba(0,0,0,0.1);
        font-size: 14px;
        font-weight: ${type === 'error' ? '600' : '500'};
        animation: slideIn 0.3s, fadeOut 0.3s 2.7s;
        opacity: 0;
        border-left: 4px solid ${type === 'success' ? '#27ae60' : type === 'error' ? '#c0392b' : '#e67e22'};
        display: flex;
        align-items: center;
      `;
      notification.style.animation = 'slideIn 0.3s forwards';
      const icon = document.createElement('div');
      icon.style = `
        margin-right: 10px;
        font-size: 18px;
        font-weight: bold;
      `;
      icon.innerHTML = type === 'success' ? 'OK' : type === 'error' ? 'ERROR' : 'WARNING';
      
      const textSpan = document.createElement('span');
      textSpan.textContent = message;
      
      notification.appendChild(icon);
      notification.appendChild(textSpan);
      
      notificationContainer.appendChild(notification);
      
      setTimeout(() => {
        notification.style.animation = 'fadeOut 0.3s forwards';
        setTimeout(() => {
          if (notificationContainer.contains(notification)) {
            notificationContainer.removeChild(notification);
          }
        }, 300);
      }, 3000);
    }
    
    function connect() {
      socket = new WebSocket('ws://' + window.location.hostname + ':81/');
      
      socket.onopen = function() {
        console.log('WebSocket connection established');
        showNotification('Connected to the parking system', 'success');
      };
      
      socket.onmessage = function(event) {
        const data = JSON.parse(event.data);
        
        if (data.type === 'discovery_status') {
          handleDiscoveryStatus(data);
        } else if (data.spots && Array.isArray(data.spots)) {
          updateParkingStatus(data);
        } else if (data.type === 'error') {
          showNotification(data.message, 'error');
          console.error('Error:', data.message);
        } else if (data.type === 'notification') {
          showNotification(data.message, data.notificationType || 'info');
          console.log('Notification:', data.message);
        } else {
          console.log('Received other message:', data);
        }
      };
      
      socket.onclose = function() {
        console.log('WebSocket connection closed');
        showNotification('Connection lost. Reconnecting...', 'error');
        setTimeout(connect, 2000);
      };
      
      document.getElementById('startDiscoveryBtn').addEventListener('click', function() {
        if (socket && socket.readyState === WebSocket.OPEN) {
          const message = JSON.stringify({ command: 'start_discovery' });
          socket.send(message);
          console.log('Start discovery request sent');
          
          const btn = document.getElementById('startDiscoveryBtn');
          btn.classList.add('bounce');
          setTimeout(() => btn.classList.remove('bounce'), 500);
        } else {
          showNotification('Connection lost. Please refresh the page.', 'error');
        }
      });
      
      document.getElementById('stopDiscoveryBtn').addEventListener('click', function() {
        if (socket && socket.readyState === WebSocket.OPEN) {
          const message = JSON.stringify({ command: 'stop_discovery' });
          socket.send(message);
          console.log('Stop discovery request sent');
          
          const btn = document.getElementById('stopDiscoveryBtn');
          btn.classList.add('bounce');
          setTimeout(() => btn.classList.remove('bounce'), 500);
        } else {
          showNotification('Connection lost. Please refresh the page.', 'error');
        }
      });
    }
    
    function handleDiscoveryStatus(data) {
      console.log('Discovery status:', data);
      const statusEl = document.getElementById('discoveryStatus');
      if (!statusEl) return;
      
      const startBtn = document.getElementById('startDiscoveryBtn');
      const stopBtn = document.getElementById('stopDiscoveryBtn');
      
      if (data.active) {
        if (data.physical_override) {
          statusEl.textContent = 'Discovery active (Physical button override)';
          statusEl.className = 'discovery-status status-override';
          if (startBtn) startBtn.disabled = true;
          if (stopBtn) stopBtn.disabled = true;
        } else {
          statusEl.textContent = 'Discovery mode active...';
          statusEl.className = 'discovery-status status-active';
          if (startBtn) startBtn.disabled = true;
          if (stopBtn) stopBtn.disabled = false;
        }
        statusEl.style.display = 'inline-block';
      } else {
        statusEl.textContent = 'Discovery mode stopped';
        statusEl.className = 'discovery-status status-inactive';
        statusEl.style.display = 'inline-block';
        setTimeout(function() {
          if (statusEl) statusEl.style.display = 'none';
        }, 3000);
        if (startBtn) startBtn.disabled = false;
        if (stopBtn) stopBtn.disabled = true;
      }
      
      if (data.message) {
        showNotification(data.message, data.active ? 'success' : 'warning');
        console.log('Status message:', data.message);
      }
    }
    
    function updateParkingStatus(data) {
      const statusGrid = document.getElementById('statusGrid');
      if (!statusGrid) return;
      
      let newHtml = '';
      
      let availableCount = 0;
      let occupiedCount = 0;
      let errorCount = 0;
      
      console.log("Received parking data:", data);
      
      if (!data.spots || !Array.isArray(data.spots)) {
        console.error("Invalid parking data format: missing spots array");
        return;
      }
      
      const changedSpots = [];
      
      data.spots.forEach(spot => {
        if (!spot) return;
        
        const spotId = spot.id;
        if (spotId === undefined || spotId === null) return;
        
        const currentServerTime = data.serverTime || 0;
        const timeSinceUpdate = currentServerTime - (spot.lastUpdate || 0);
        const isOffline = currentServerTime > 0 && timeSinceUpdate > 10000;
        
        const previousSpot = lastParkingState[spotId];
        const statusChanged = previousSpot && 
          (previousSpot.isOffline !== isOffline || 
           previousSpot.hasSensorError !== !!spot.sensorError ||
           (!isOffline && !spot.sensorError && previousSpot.isOccupied !== spot.isOccupied));
        
        lastParkingState[spotId] = { 
          isOffline: isOffline,
          hasSensorError: !!spot.sensorError,
          isOccupied: !!spot.isOccupied 
        };
        
        if (statusChanged) {
          changedSpots.push(spotId);
          
          if (previousSpot) {
            if (!previousSpot.isOffline && isOffline) {
              showNotification(`Spot ${spotId} went offline`, 'warning');
            } else if (previousSpot.isOffline && !isOffline) {
              showNotification(`Spot ${spotId} is back online`, 'success');
            } else if (!previousSpot.hasSensorError && spot.sensorError) {
              showNotification(`Sensor error detected on Spot ${spotId}`, 'error');
            } else if (previousSpot.hasSensorError && !spot.sensorError) {
              showNotification(`Sensor on Spot ${spotId} is working again`, 'success');
            }
          }
        }
        
        let spotClass = '';
        let spotStatus = '';
        
        if (isOffline) {
          spotClass = 'parking-spot spot-offline';
          spotStatus = 'Offline';
        } else if (spot.sensorError) {
          spotClass = 'parking-spot spot-error';
          spotStatus = 'Sensor Error';
          errorCount++;
        } else if (spot.isOccupied) {
          spotClass = 'parking-spot spot-occupied';
          spotStatus = 'Occupied';
          occupiedCount++;
        } else {
          spotClass = 'parking-spot spot-available';
          spotStatus = 'Available';
          availableCount++;
        }
        
        const distance = spot.distance !== undefined ? spot.distance : null;
        const distanceText = distance !== null ? distance + ' cm' : 'N/A';
        
        newHtml += `
          <div id="spot-${spotId}" class="${spotClass}">
            <div class="spot-header">
              <span>${spotStatus}</span>
            </div>
            <div class="spot-body">
              <div class="spot-id">Spot ${spotId}</div>
              <div class="spot-status">
                ${isOffline ? 'Last seen: ' + formatTimeSince(timeSinceUpdate) + ' ago' : 
                  spot.sensorError ? 'Sensor Error: Check hardware' : 
                  'Distance: ' + distanceText}
              </div>
            </div>
          </div>
        `;
      });
      
      // Update the grid
      statusGrid.innerHTML = newHtml;
      
      setTimeout(() => {
        changedSpots.forEach(id => {
          const spotElement = document.getElementById(`spot-${id}`);
          if (spotElement) {
            spotElement.classList.add('bounce');
            setTimeout(() => {
              if (spotElement.classList) {
                spotElement.classList.remove('bounce');
              }
            }, 1000);
          }
        });
      }, 50);
      
      animateCounter('totalSpots', data.spots.length);
      animateCounter('availableSpots', availableCount);
      animateCounter('occupiedSpots', occupiedCount);
      animateCounter('errorSpots', errorCount);
      
      updateDeviceManagement(data.spots, data.serverTime);
    }
    
    function formatTimeSince(ms) {
      const seconds = Math.floor(ms / 1000);
      if (seconds < 60) return `${seconds}s`;
      if (seconds < 3600) return `${Math.floor(seconds / 60)}m`;
      return `${Math.floor(seconds / 3600)}h`;
    }
    
    function animateCounter(elementId, newValue) {
      const element = document.getElementById(elementId);
      const currentValue = parseInt(element.textContent);
      
      if (currentValue !== newValue) {
        element.classList.add('bounce');
        setTimeout(() => element.classList.remove('bounce'), 500);
        element.textContent = newValue;
      }
    }
    
    function updateDeviceManagement(spots, serverTime) {
      const deviceList = document.getElementById('deviceList');
      let newHtml = '';
      
      if (!spots || !Array.isArray(spots) || spots.length === 0) {
        deviceList.innerHTML = `
          <div class="empty-state" style="text-align: center; padding: 30px 0;">
            <p style="color: #7f8c8d; font-size: 0.9rem;">No devices registered</p>
            <p style="color: #95a5a6; font-size: 0.8rem; margin-top: 5px;">
              Use the discovery mode to find and register devices
            </p>
          </div>
        `;
        return;
      }
      
      spots.sort((a, b) => a.id - b.id);
      
      spots.forEach(device => {
        // Use the isOffline flag sent by the server if available, 
        // otherwise fallback to the old time-based calculation
        const currentServerTime = serverTime || 0;
        const timeSinceUpdate = currentServerTime - device.lastUpdate;
        const isOffline = device.isOffline !== undefined 
          ? device.isOffline 
          : (currentServerTime > 0 && timeSinceUpdate > ${DEVICE_OFFLINE_TIMEOUT});
        
        const deviceMac = device.mac || 'Unknown';
        const offlineDuration = timeSinceUpdate > 60000 
          ? Math.floor(timeSinceUpdate / 60000) + ' min' 
          : Math.floor(timeSinceUpdate / 1000) + ' sec';
        
        newHtml += `
          <div class="device-item ${isOffline ? 'device-offline' : ''}">
            <div class="device-info">
              <span class="device-id">ID ${device.id}</span>
              <span class="device-mac">${deviceMac}</span>
              <span class="device-status ${isOffline ? 'status-offline' : device.sensorError ? 'status-error' : 'status-online'}">
                <span class="status-indicator"></span>
                ${isOffline ? 'Offline (' + offlineDuration + ')' : device.sensorError ? 'Sensor Error' : 'Online'}
              </span>
            </div>
            <div class="device-buttons">
              <button onclick="openDeviceEditor({id: ${device.id}, mac: '${deviceMac}'})">
                Edit
              </button>
            </div>
          </div>
        `;
      });
      
      deviceList.innerHTML = newHtml;
    }
    
    function openDeviceEditor(device) {
      const editor = document.getElementById('deviceEditor');
      if (!editor || !device) return;
      
      editor.style.display = 'block';
      
      const editDeviceIdElement = document.getElementById('editDeviceId');
      if (editDeviceIdElement) {
        editDeviceIdElement.textContent = device.id;
      }
      
      const editDeviceMacElement = document.getElementById('editDeviceMac');
      if (editDeviceMacElement) {
        editDeviceMacElement.textContent = device.mac || 'Unknown';
      }
      
      const newDeviceIdElement = document.getElementById('newDeviceId');
      if (newDeviceIdElement) {
        newDeviceIdElement.value = device.id;
      }
      
      window.currentEditDevice = device;
      
      editor.scrollIntoView({ behavior: 'smooth' });
    }
    
    function sendDeviceCommand(command, deviceData) {
      if (socket && socket.readyState === WebSocket.OPEN) {
        const message = JSON.stringify({
          command: command,
          ...deviceData
        });
        socket.send(message);
        console.log(`Sent ${command} command:`, message);
      } else {
        showNotification('Connection to server lost. Please refresh the page and try again.', 'error');
      }
    }
    
    window.onload = function() {
      // Add CSS keyframes dynamically
      const style = document.createElement('style');
      style.textContent = `
        @keyframes slideIn {
          from { opacity: 0; transform: translateX(20px); }
          to { opacity: 1; transform: translateX(0); }
        }
        @keyframes fadeOut {
          from { opacity: 1; }
          to { opacity: 0; }
        }
      `;
      document.head.appendChild(style);
      
      connect();
      
      const startBtn = document.getElementById('startDiscoveryBtn');
      const stopBtn = document.getElementById('stopDiscoveryBtn');
      if (startBtn) startBtn.disabled = false;
      if (stopBtn) stopBtn.disabled = true;
      
      const saveDeviceBtn = document.getElementById('saveDeviceBtn');
      if (saveDeviceBtn) {
        saveDeviceBtn.addEventListener('click', function() {
          const originalDevice = window.currentEditDevice;
          if (!originalDevice) return;
          
          const newIdElement = document.getElementById('newDeviceId');
          if (!newIdElement) return;
          
          const newId = parseInt(newIdElement.value);
          
          if (isNaN(newId) || newId < 1 || newId > 10) {
            showNotification('Please enter a valid ID between 1 and 10', 'error');
            return;
          }
          
          sendDeviceCommand('update_device_id', {
            mac: originalDevice.mac,
            oldId: originalDevice.id,
            newId: newId
          });
          
          const deviceEditor = document.getElementById('deviceEditor');
          if (deviceEditor) deviceEditor.style.display = 'none';
          
          showNotification(`Device ID updated from ${originalDevice.id} to ${newId}`, 'success');
        });
      }
      
      const removeDeviceBtn = document.getElementById('removeDeviceBtn');
      if (removeDeviceBtn) {
        removeDeviceBtn.addEventListener('click', function() {
          const originalDevice = window.currentEditDevice;
          if (!originalDevice) return;
          
          if (confirm(`Are you sure you want to remove device ID ${originalDevice.id}?`)) {
            sendDeviceCommand('remove_device', {
              mac: originalDevice.mac,
              id: originalDevice.id
            });
            
            const deviceEditor = document.getElementById('deviceEditor');
            if (deviceEditor) deviceEditor.style.display = 'none';
            
            showNotification(`Device ID ${originalDevice.id} removed`, 'success');
          }
        });
      }
      
      const cancelEditBtn = document.getElementById('cancelEditBtn');
      if (cancelEditBtn) {
        cancelEditBtn.addEventListener('click', function() {
          const deviceEditor = document.getElementById('deviceEditor');
          if (deviceEditor) deviceEditor.style.display = 'none';
        });
      }
    };
  </script>
</body>
</html>
)html";

TaskHandle_t WebUITask;
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

void broadcastParkingStatus() {
  DynamicJsonDocument doc(2048);
  doc["type"] = "parking_status";
  JsonArray spots = doc.createNestedArray("spots");

  unsigned long currentTime = millis();
  doc["serverTime"] = currentTime;

  for (int i = 0; i < connectedSpots; i++) {
    JsonObject spot = spots.createNestedObject();
    spot["id"] = parkingSpots[i].id;
    spot["mac"] = parkingSpots[i].macAddress;
    spot["distance"] = parkingSpots[i].distance;
    spot["isOccupied"] = parkingSpots[i].isOccupied;
    spot["lastUpdate"] = parkingSpots[i].lastUpdate;
    spot["sensorError"] = parkingSpots[i].hasSensorError;
    
    // Calculate if the device is offline based on the time since last update
    unsigned long timeSinceLastUpdate = currentTime - parkingSpots[i].lastUpdate;
    bool isOffline = timeSinceLastUpdate > DEVICE_OFFLINE_TIMEOUT;
    spot["isOffline"] = isOffline;
  }

  String jsonString;
  serializeJson(doc, jsonString);
  webSocket.broadcastTXT(jsonString);
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

            broadcastParkingStatus();
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

            broadcastParkingStatus();
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
              Serial.println("Spot status changed, broadcasting update to all clients");
              broadcastParkingStatus();
            }
          }
        }
      }
      break;
  }
}

void webUITaskFunction(void* parameter) {
  for (;;) {
    webSocket.loop();
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

        String responseJson;
        serializeJson(responseDoc, responseJson);
        webSocket.broadcastTXT(responseJson);
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
          responseDoc["message"] = wifiOK ? "Discovery mode activated due to multiple slaves offline" : "Discovery mode activated due to WiFi connection issue";

          String responseJson;
          serializeJson(responseDoc, responseJson);
          webSocket.broadcastTXT(responseJson);

          firstOfflineTime = 0;
          offlineCount = 0;
        }
      }

      previousActiveSpots = activeSpots;

      broadcastParkingStatus();
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


  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send_P(200, "text/html", mainPage);
  });

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
    webUITaskFunction,
    "WebUITask",
    10000,
    NULL,
    1,
    &WebUITask,
    1);

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
    
  // Initial health ping will be handled by the API task
  // No need to send health ping here since API task will handle it
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

bool checkForSystemErrors() {
  bool hasSensorErrors = false;
  bool hasSlaveConnectionErrors = false;
  bool hasMasterConnectionErrors = false;
  unsigned long currentTime = millis();
  
  // Check for sensor errors in slaves
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
  unsigned long currentTime = millis();
  static unsigned long lastWiFiCheck = 0;

  checkDiscoveryButton();

  bool buttonState = digitalRead(DISCOVERY_BUTTON_PIN);

  if (buttonState == LOW) {
    physicalOverrideActive = true;
    discoveryMode = true;
  } else {
    physicalOverrideActive = false;
  }

  if (wifiDisconnectionDetected && currentTime - lastWiFiCheck >= 30000) { // Keep this at 30s, no need to use DEVICE_OFFLINE_TIMEOUT
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
  
  // API-related tasks now handled by ApiTask on Core 1
  
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
  
  // Check if the device is offline based on timestamp
  unsigned long currentTime = millis();
  unsigned long timeSinceLastUpdate = currentTime - parkingSpots[index].lastUpdate;
  bool isOffline = timeSinceLastUpdate > DEVICE_OFFLINE_TIMEOUT;
  
  String systemStatus = isOffline ? "offline" : "online";
  
  DynamicJsonDocument doc(256);
  doc["parkingStatus"] = parkingStatus;
  doc["sensorError"] = hasSensorError;
  doc["systemStatus"] = systemStatus;
  
  // Add timestamp info for debugging
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
  
  // Check if the device is offline based on timestamp
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
