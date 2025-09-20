import { ref, set, update, get, onValue, push, query, orderByChild, limitToLast, serverTimestamp } from 'firebase/database';
import { database } from './firebase';

// Update parking spot status
export const updateParkingSpot = async (deviceId, data) => {
  try {
    const spotRef = ref(database, `SParking/Device${deviceId}`);
    await update(spotRef, data);
    return { success: true };
  } catch (error) {
    console.error('Error updating parking spot:', error);
    return { success: false, error: error.message };
  }
};

// Get specific parking spot data
export const getParkingSpot = async (deviceId) => {
  try {
    const spotRef = ref(database, `SParking/Device${deviceId}`);
    const snapshot = await get(spotRef);
    if (snapshot.exists()) {
      return { success: true, data: snapshot.val() };
    } else {
      return { success: false, error: 'Parking spot not found' };
    }
  } catch (error) {
    console.error('Error getting parking spot:', error);
    return { success: false, error: error.message };
  }
};

// Get all parking spots
export const getAllParkingSpots = async () => {
  try {
    const spotsRef = ref(database, 'SParking');
    const snapshot = await get(spotsRef);
    if (snapshot.exists()) {
      return { success: true, data: snapshot.val() };
    } else {
      return { success: true, data: {} }; // Return empty object if no spots found
    }
  } catch (error) {
    console.error('Error getting all parking spots:', error);
    return { success: false, error: error.message };
  }
};

// Initialize parking spot in database with default values
export const initializeParkingSpot = async (deviceId) => {
  try {
    const spotRef = ref(database, `SParking/Device${deviceId}`);
    await set(spotRef, {
      'Parking Status': 'open',
      'Sensor Error': false,
      'System Status': 'online'
    });
    return { success: true };
  } catch (error) {
    console.error('Error initializing parking spot:', error);
    return { success: false, error: error.message };
  }
};

// Log API request from ESP32 to database
export const logApiRequest = async (deviceId, endpoint, method, body, success, errorMessage = null) => {
  try {
    const logRef = ref(database, 'SParkingApiLogs');
    await push(logRef, {
      deviceId,
      endpoint,
      method,
      body: JSON.stringify(body),
      success,
      errorMessage,
      timestamp: serverTimestamp()
    });
    return { success: true };
  } catch (error) {
    console.error('Error logging API request:', error);
    return { success: false, error: error.message };
  }
};

// Get API logs (limited to most recent entries)
export const getApiLogs = async (limit = 50) => {
  try {
    const logsQuery = query(
      ref(database, 'SParkingApiLogs'),
      orderByChild('timestamp'),
      limitToLast(limit)
    );
    
    const snapshot = await get(logsQuery);
    if (snapshot.exists()) {
      const logs = [];
      snapshot.forEach((childSnapshot) => {
        logs.push({
          id: childSnapshot.key,
          ...childSnapshot.val()
        });
      });
      // Sort by timestamp descending (most recent first)
      logs.sort((a, b) => b.timestamp - a.timestamp);
      return { success: true, data: logs };
    } else {
      return { success: true, data: [] };
    }
  } catch (error) {
    console.error('Error getting API logs:', error);
    return { success: false, error: error.message };
  }
};

// Subscribe to real-time API log updates
export const subscribeToApiLogs = (callback, limit = 20) => {
  const logsQuery = query(
    ref(database, 'SParkingApiLogs'),
    orderByChild('timestamp'),
    limitToLast(limit)
  );
  
  const unsubscribe = onValue(logsQuery, (snapshot) => {
    const logs = [];
    if (snapshot.exists()) {
      snapshot.forEach((childSnapshot) => {
        logs.push({
          id: childSnapshot.key,
          ...childSnapshot.val()
        });
      });
      // Sort by timestamp descending (most recent first)
      logs.sort((a, b) => (b.timestamp || 0) - (a.timestamp || 0));
    }
    callback(logs);
  });
  
  return unsubscribe; // Return the unsubscribe function
};