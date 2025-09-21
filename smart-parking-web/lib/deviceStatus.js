import db from './firebaseAdmin';

// The maximum time allowed since last health ping before considering a device offline (in milliseconds)
const MAX_PING_AGE = 60000; // 1 minute

/**
 * Checks if a device is online based on its last health ping timestamp
 * @param {string} deviceId - The ID of the device to check
 * @returns {Promise<boolean>} - True if the device is online, false if offline
 */
export async function isDeviceOnline(deviceId) {
  try {
    const deviceRef = db.ref(`SParking/Device${deviceId}`);
    const snapshot = await deviceRef.child('lastHealthPing').once('value');
    const lastPing = snapshot.val();
    
    if (!lastPing) {
      // If no health ping has been recorded, consider the device offline
      return false;
    }
    
    // Calculate time since last ping
    const lastPingTime = new Date(lastPing).getTime();
    const currentTime = new Date().getTime();
    const timeSinceLastPing = currentTime - lastPingTime;
    
    return timeSinceLastPing <= MAX_PING_AGE;
  } catch (error) {
    console.error(`Error checking online status for device ${deviceId}:`, error);
    return false;
  }
}

/**
 * Updates the online status of all devices in the database based on health ping timestamps
 */
export async function updateAllDevicesOnlineStatus() {
  try {
    // Get all devices
    const snapshot = await db.ref('SParking').once('value');
    const devices = snapshot.val() || {};
    
    // Check each device
    for (const deviceKey in devices) {
      if (deviceKey.startsWith('Device')) {
        const deviceId = deviceKey.replace('Device', '');
        const device = devices[deviceKey];
        
        if (!device.lastHealthPing) continue;
        
        const lastPingTime = new Date(device.lastHealthPing).getTime();
        const currentTime = new Date().getTime();
        const timeSinceLastPing = currentTime - lastPingTime;
        
        // Update online status
        const isOnline = timeSinceLastPing <= MAX_PING_AGE;
        await db.ref(`SParking/${deviceKey}/isOnline`).set(isOnline);
      }
    }
    
    console.log('Updated online status for all devices');
  } catch (error) {
    console.error('Error updating devices online status:', error);
  }
}