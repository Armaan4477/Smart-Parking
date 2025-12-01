import { ref, set, update, get, onValue, push, query, orderByChild, limitToLast, serverTimestamp, off } from 'firebase/database';
import { database } from './firebase';
import { KILL_SWITCH_ENABLED } from '../context/KillSwitchContext';

// Subscribe to real-time updates for all parking spots
export const subscribeToAllParkingSpots = (callback) => {
  if (KILL_SWITCH_ENABLED) {
    console.warn('Firebase communication is disabled by kill switch');
    callback({ success: false, error: 'Firebase communication is disabled' });
    return null;
  }
  
  const spotsRef = ref(database, 'SParking');
  
  // Set up the listener for real-time updates
  onValue(spotsRef, (snapshot) => {
    if (snapshot.exists()) {
      callback({ success: true, data: snapshot.val() });
    } else {
      callback({ success: true, data: {} }); // Return empty object if no spots found
    }
  }, (error) => {
    console.error('Error subscribing to parking spots:', error);
    callback({ success: false, error: error.message });
  });

  // Return reference so it can be used to unsubscribe
  return spotsRef;
};

// Subscribe to device configuration updates
export const subscribeToDeviceConfig = (callback) => {
  if (KILL_SWITCH_ENABLED) {
    console.warn('Firebase communication is disabled by kill switch');
    callback({ success: false, error: 'Firebase communication is disabled' });
    return null;
  }
  
  const configRef = ref(database, 'SConfig');
  
  onValue(configRef, (snapshot) => {
    if (snapshot.exists()) {
      callback({ success: true, data: snapshot.val() });
    } else {
      callback({ success: true, data: {} }); // Return empty object if no config found
    }
  }, (error) => {
    console.error('Error subscribing to device config:', error);
    callback({ success: false, error: error.message });
  });

  return configRef;
};

// Unsubscribe from real-time updates
export const unsubscribeFromParkingSpots = (spotsRef) => {
  if (spotsRef) {
    off(spotsRef);
  }
};

// Update parking spot status
export const updateParkingSpot = async (deviceId, data) => {
  if (KILL_SWITCH_ENABLED) {
    console.warn('Firebase communication is disabled by kill switch');
    return { success: false, error: 'Firebase communication is disabled' };
  }
  
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
  if (KILL_SWITCH_ENABLED) {
    console.warn('Firebase communication is disabled by kill switch');
    return { success: false, error: 'Firebase communication is disabled' };
  }
  
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
  if (KILL_SWITCH_ENABLED) {
    console.warn('Firebase communication is disabled by kill switch');
    return { success: false, error: 'Firebase communication is disabled' };
  }
  
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
  if (KILL_SWITCH_ENABLED) {
    console.warn('Firebase communication is disabled by kill switch');
    return { success: false, error: 'Firebase communication is disabled' };
  }
  
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

// Log API request functionality moved to logHandler.js
// to keep logs in memory only and not write to Firebase

// API logs functionality moved to logHandler.js
// to keep logs in memory only and not fetch from Firebase

// Get threshold configuration for all devices or specific device
export const getThresholdConfig = async (deviceId = null) => {
  if (KILL_SWITCH_ENABLED) {
    console.warn('Firebase communication is disabled by kill switch');
    return { success: false, error: 'Firebase communication is disabled' };
  }
  
  try {
    if (deviceId) {
      // Get threshold for specific device
      const configRef = ref(database, `SConfig/thresholds/Device${deviceId}`);
      const snapshot = await get(configRef);
      
      if (snapshot.exists()) {
        return { success: true, data: snapshot.val() };
      } else {
        // If no specific threshold for this device, get default threshold
        const defaultRef = ref(database, 'SConfig/thresholds/default');
        const defaultSnapshot = await get(defaultRef);
        
        if (defaultSnapshot.exists()) {
          return { success: true, data: defaultSnapshot.val() };
        } else {
          // Return fallback value if no configuration exists
          return { success: true, data: { value: 45 } }; 
        }
      }
    } else {
      // Get all threshold configurations
      const configRef = ref(database, 'SConfig/thresholds');
      const snapshot = await get(configRef);
      
      if (snapshot.exists()) {
        return { success: true, data: snapshot.val() };
      } else {
        // Initialize default configuration if none exists
        await set(ref(database, 'SConfig/thresholds/default'), { value: 45 });
        return { success: true, data: { default: { value: 45 } } };
      }
    }
  } catch (error) {
    console.error('Error getting threshold configuration:', error);
    return { success: false, error: error.message };
  }
};

// Update threshold configuration for all devices or specific device
export const updateThresholdConfig = async (value, deviceId = null) => {
  if (KILL_SWITCH_ENABLED) {
    console.warn('Firebase communication is disabled by kill switch');
    return { success: false, error: 'Firebase communication is disabled' };
  }
  
  try {
    if (!value || isNaN(value) || value < 0 || value > 200) {
      return { success: false, error: 'Invalid threshold value. Must be a number between 0 and 200.' };
    }
    
    const numValue = parseInt(value, 10);
    
    if (deviceId) {
      // Update threshold for specific device
      const configRef = ref(database, `SConfig/thresholds/Device${deviceId}`);
      await set(configRef, { value: numValue, updatedAt: serverTimestamp() });
    } else {
      // Update default threshold for all devices
      const configRef = ref(database, 'SConfig/thresholds/default');
      await set(configRef, { value: numValue, updatedAt: serverTimestamp() });
    }
    
    return { success: true };
  } catch (error) {
    console.error('Error updating threshold configuration:', error);
    return { success: false, error: error.message };
  }
};