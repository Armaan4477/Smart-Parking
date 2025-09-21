import db from './firebaseAdmin';

// The maximum time allowed since last health ping before considering a device offline (in milliseconds)
const MAX_PING_AGE = 40000; // 40 seconds

/**
 * Checks if a device is online based on its status or last health ping timestamp
 * @param {string} deviceId - The ID of the device to check
 * @returns {Promise<boolean>} - True if the device is online, false if offline
 */
export async function isDeviceOnline(deviceId) {
  try {
    const deviceRef = db.ref(`SParking/Device${deviceId}`);
    
    // First check if System Status field exists and use that
    const systemStatusSnapshot = await deviceRef.child('System Status').once('value');
    if (systemStatusSnapshot.exists()) {
      const systemStatus = systemStatusSnapshot.val();
      if (systemStatus === 'online' || systemStatus === 'offline') {
        return systemStatus === 'online';
      }
    }
    
    // Fall back to checking health ping if System Status is not set
    const lastPingSnapshot = await deviceRef.child('lastHealthPing').once('value');
    const lastPing = lastPingSnapshot.val();
    
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
 * and System Status field. Optimized to only update when status changes to reduce database operations.
 */
export async function updateAllDevicesOnlineStatus() {
  try {
    // Get all devices
    const snapshot = await db.ref('SParking').once('value');
    const devices = snapshot.val() || {};
    
    // Track devices needing updates to batch them if possible
    const updates = {};
    let updatedAny = false;
    
    // Check each device
    for (const deviceKey in devices) {
      if (deviceKey.startsWith('Device')) {
        const device = devices[deviceKey];
        
        // First check if we have a System Status field and use that
        if (device['System Status'] === 'offline') {
          // If System Status is already set to offline, use that
          if (device.isOnline !== false) {
            updates[`${deviceKey}/isOnline`] = false;
            updatedAny = true;
          }
          continue;
        }
        
        // If no health ping recorded, check if we need to update
        if (!device.lastHealthPing) {
          if (device.isOnline !== false) {
            updates[`${deviceKey}/isOnline`] = false;
            updatedAny = true;
          }
          continue;
        }
        
        // Calculate time since last ping
        const lastPingTime = new Date(device.lastHealthPing).getTime();
        const currentTime = new Date().getTime();
        const timeSinceLastPing = currentTime - lastPingTime;
        
        // Calculate new online status
        const isOnline = timeSinceLastPing <= MAX_PING_AGE && device['System Status'] !== 'offline';
        
        // Only update if the status has changed or if isOnline property doesn't exist
        if (device.isOnline === undefined || device.isOnline !== isOnline) {
          updates[`${deviceKey}/isOnline`] = isOnline;
          updatedAny = true;
          
          // Also update System Status if needed
          if (device['System Status'] !== (isOnline ? 'online' : 'offline')) {
            updates[`${deviceKey}/System Status`] = isOnline ? 'online' : 'offline';
          }
        }
      }
    }
    
    // Apply batch updates if any device status has changed
    if (updatedAny) {
      await db.ref('SParking').update(updates);
      console.log('Updated online status for devices with changes');
    } else {
      console.log('No device status changes detected');
    }
  } catch (error) {
    console.error('Error updating devices online status:', error);
  }
}