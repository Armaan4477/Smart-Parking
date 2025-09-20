# Smart Parking System

A comprehensive IoT solution for monitoring parking space availability using ESP32, ESP8266, and Firebase.

## System Architecture

This smart parking system consists of:

1. **ESP8266 Slave Devices**: Placed at each parking spot with ultrasonic sensors to detect vehicles
2. **ESP32 Master Device**: Coordinates with slave devices via WebSockets
3. **Next.js API Server**: Receives data from ESP32 and updates Firebase
4. **Firebase Realtime Database**: Stores parking space status
5. **Next.js Frontend** (future): Web interface to visualize parking data

## Directory Structure

```
Smart-Parking/
├── smart_parking_master_esp32/
│   ├── smart_parking_master_esp32.ino (Original code)
│   └── smart_parking_master_esp32_with_api.ino (Updated with API integration)
├── smart_parking_slave_esp8266/
│   └── smart_parking_slave_esp8266.ino
├── smart-parking-web/
│   ├── pages/
│   │   ├── api/
│   │   │   └── parking/ (API endpoints for ESP32 communication)
│   │   └── ... (Frontend pages)
│   ├── lib/
│   │   ├── firebase.js
│   │   ├── firebaseAdmin.js
│   │   ├── firebaseConfig.js
│   │   └── database.js
│   └── ... (Next.js config files)
├── TESTING_GUIDE.md
└── README.md
```

## Setup Instructions

### Hardware Setup

1. **ESP32 Master**:
   - Ultrasonic sensor (HC-SR04) for distance measurements
   - LED indicators for system status
   - Discovery button

2. **ESP8266 Slave**:
   - Ultrasonic sensor (HC-SR04) connected to pins D1 (trigger) and D2 (echo)
   - Red LED on D4 (occupied indicator)
   - Green LED on D3 (available indicator)

### Software Setup

#### 1. ESP8266 Slave Devices
- Flash `smart_parking_slave_esp8266.ino` to each ESP8266
- Update WiFi credentials if necessary

#### 2. ESP32 Master Device
- Flash `smart_parking_master_esp32_with_api.ino` to the ESP32
- Update WiFi credentials if necessary
- Update `API_BASE_URL` with your deployed Next.js API URL

#### 3. Next.js Web Application
- Navigate to the `smart-parking-web` directory
- Install dependencies: `npm install`
- Set up Firebase configuration files:
  - Copy `lib/firebaseConfig.example.js` to `lib/firebaseConfig.js` and update with your Firebase credentials
  - Copy `lib/serviceAccountKey.example.json` to `lib/serviceAccountKey.json` and update with your service account key
  - **Important**: Never commit these files with real credentials to your repository
- Run locally: `npm run dev`
- Deploy to production: Follow your preferred hosting provider's instructions

## Firebase Data Structure

```
SParking {
  Device1 {
    "Parking Status": "open" or "occupied",
    "Sensor Error": false,
    "System Status": "online"
  },
  Device2 {
    ...
  }
}
```

## API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/parking` | GET | Get all parking spots |
| `/api/parking/:deviceId` | GET | Get specific parking spot data |
| `/api/parking/:deviceId` | PUT | Update parking spot data |
| `/api/parking/init/:deviceId` | POST | Initialize a parking spot in Firebase |
| `/api/parking/sensor/:deviceId` | POST | Update sensor data for a parking spot |

## Future Enhancements

1. User authentication for the dashboard
2. Mobile app integration
3. Historical data analytics
4. Reservation system
5. Integration with payment systems

## Testing

See [TESTING_GUIDE.md](./TESTING_GUIDE.md) for detailed instructions on testing the integration.

## License

This project is licensed under the MIT License - see the LICENSE file for details.