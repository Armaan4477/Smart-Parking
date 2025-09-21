# Smart Parking System Integration Testing Guide

This guide provides instructions for testing the complete integration between:
1. ESP32 Master device
2. ESP8266 Slave devices
3. Next.js API server
4. Firebase Realtime Database

## Prerequisites

- ESP32 Master device flashed with `smart_parking_master_esp32_with_api.ino`
- ESP8266 Slave device(s) flashed with `smart_parking_slave_esp8266.ino`
- Firebase project set up with Realtime Database
- Next.js application deployed (or running locally)

## Setup Steps

### 1. Set Up Firebase

- Replace all placeholder values in `lib/firebaseConfig.js` with your actual Firebase project details
- Replace the contents of `lib/serviceAccountKey.json` with your downloaded service account key
- Update the `databaseURL` in `lib/firebaseAdmin.js` with your actual Firebase database URL

### 2. Deploy the Next.js API

#### For Local Testing:
```bash
cd smart-parking-web
npm install
npm run dev
```
This will start the server at http://localhost:3000

#### For Production:
Deploy to Vercel, Netlify, or your preferred hosting service.

### 3. Update ESP32 Code with API URL

- Edit `smart_parking_master_esp32_with_api.ino`
- Update the `API_BASE_URL` constant with your deployed API URL:
  ```cpp
  const char* API_BASE_URL = "https://your-deployed-app.vercel.app/api/parking";
  // or for local testing (from a different device on the same network):
  // const char* API_BASE_URL = "http://192.168.1.xxx:3000/api/parking";
  ```

## Testing the Integration

### 1. Deploy the Next.js application

If testing locally:
```bash
cd smart-parking-web
npm run dev
```

### 2. Power up the ESP32 Master

- Connect the ESP32 to power
- Monitor the serial output to ensure it connects to WiFi
- Verify it successfully initializes communication with the API

### 3. Power up ESP8266 Slave device(s)

- Connect the ESP8266(s) to power
- Press the discovery button on the ESP32 to enter discovery mode
- Verify that the ESP8266(s) connect to the ESP32 master

### 4. Verify Real-time Updates

- Monitor the serial output of both ESP32 and ESP8266
- Trigger the ultrasonic sensor on the ESP8266 by placing an object in front of it
- Verify that:
  1. The ESP8266 detects the object and sends data to ESP32
  2. The ESP32 receives the data and updates its internal state
  3. The ESP32 sends the data to the Next.js API
  4. The API updates the Firebase Realtime Database

### 5. Check Firebase Database

- Open the Firebase console
- Navigate to the Realtime Database
- Verify that the data structure matches the expected format:
  ```
  SParking {
    Device1 {
      "Parking Status": "open" or "occupied",
      "Sensor Error": true or false,
      "System Status": "online" or "offline"
    }
  }
  ```
- Confirm that changes are reflected in real-time when the sensor status changes

### 6. Test Error Handling

- Disconnect the ESP8266 to simulate a slave going offline
- Verify that the ESP32 detects this and updates the system status in Firebase
- Reconnect the ESP8266 and verify recovery
- Block the ultrasonic sensor to simulate a sensor error
- Verify that the error is properly detected and reported to Firebase

## Troubleshooting

### API Connection Issues
- Verify that the API_BASE_URL is correctly set in the ESP32 code
- Check that your API is accessible from the ESP32's network
- Inspect ESP32's serial output for HTTP response codes

### Database Issues
- Verify Firebase credentials and permissions
- Check Firebase Rules to ensure read/write access

### WebSocket Communication Issues
- Ensure ESP8266s are connecting to the ESP32 master
- Check the serial output on both devices for connection status

## Next Steps

Once the integration is verified:
1. Build a front-end dashboard using Next.js pages to visualize parking data
2. Implement user authentication for the dashboard
3. Add analytics features to track parking space usage over time