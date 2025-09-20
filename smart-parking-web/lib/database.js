import { ref, set, update, get, onValue } from 'firebase/database';
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