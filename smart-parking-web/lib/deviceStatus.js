import db from './firebaseAdmin';
import { logApiRequest } from './logHandler';
import { KILL_SWITCH_ENABLED } from '../context/KillSwitchContext';

// The maximum time allowed since last health ping before considering a device offline (in milliseconds)
const MAX_PING_AGE = 40000; // 40 seconds

/**
 * Checks if a device is online based on its status or last health ping timestamp
 * @param {string} deviceId - The ID of the device to check
 * @returns {Promise<boolean>} - True if the device is online, false if offline
 */
export async function isDeviceOnline(deviceId) {
  if (KILL_SWITCH_ENABLED) {
    console.warn('Firebase communication is disabled by kill switch');
    return false;
  }
  
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
 * Also updates master ESP32 discovery mode status if it has timed out.
 */
/**
 * Gets the last seen time for a device
 * @param {string} deviceId - The ID of the device to check
 * @returns {Promise<string|null>} - ISO string of last seen time or null if never seen
 */
export async function getDeviceLastSeen(deviceId) {
  if (KILL_SWITCH_ENABLED) {
    console.warn('Firebase communication is disabled by kill switch');
    return null;
  }
  
  try {
    const deviceRef = db.ref(`SParking/Device${deviceId}`);
    
    // First check if the device is online
    const isOnline = await isDeviceOnline(deviceId);
    if (isOnline) {
      // If the device is online, use the current time
      return new Date().toISOString();
    }
    
    // If offline, check for lastSeen field
    const lastSeenSnapshot = await deviceRef.child('lastSeen').once('value');
    if (lastSeenSnapshot.exists()) {
      return lastSeenSnapshot.val();
    }
    
    // If no lastSeen, try to use lastHealthPing
    const lastPingSnapshot = await deviceRef.child('lastHealthPing').once('value');
    if (lastPingSnapshot.exists()) {
      return lastPingSnapshot.val();
    }
    
    // No record of this device ever being seen
    return null;
  } catch (error) {
    console.error(`Error getting last seen time for device ${deviceId}:`, error);
    return null;
  }
}

/**
 * Updates the timeSinceUpdateMs field for all offline devices
 * Should be called periodically to keep the "Last seen" information current
 */
export async function updateOfflineDeviceTimers() {
  if (KILL_SWITCH_ENABLED) {
    console.warn('Firebase communication is disabled by kill switch');
    return;
  }
  
  try {
    // Get all devices
    const snapshot = await db.ref('SParking').once('value');
    const devices = snapshot.val() || {};
    
    // Track devices needing updates
    const updates = {};
    let updatedAny = false;
    
    // Process each device
    for (const deviceKey in devices) {
      if (deviceKey.startsWith('Device')) {
        const device = devices[deviceKey];
        
        // Only update offline devices that have a lastSeen timestamp
        if (device.isOnline === false && device.lastSeen) {
          const lastSeenTime = new Date(device.lastSeen).getTime();
          const currentTime = new Date().getTime();
          updates[`${deviceKey}/timeSinceUpdateMs`] = currentTime - lastSeenTime;
          updatedAny = true;
        }
      }
    }
    
    // Apply updates if any
    if (updatedAny) {
      await db.ref('SParking').update(updates);
      console.log('Updated timeSinceUpdateMs for offline devices');
    }
  } catch (error) {
    console.error('Error updating offline device timers:', error);
  }
}

export async function updateAllDevicesOnlineStatus() {
  if (KILL_SWITCH_ENABLED) {
    console.warn('Firebase communication is disabled by kill switch');
    return;
  }
  
  try {
    // Get all devices
    const snapshot = await db.ref('SParking').once('value');
    const devices = snapshot.val() || {};
    
    // Track devices needing updates to batch them if possible
    const updates = {};
    let updatedAny = false;
    
    // Check if discovery mode should be auto-disabled (timed out after 60 seconds)
    if (devices.Devicemaster && devices.Devicemaster.discoveryMode === true) {
      const lastUpdate = devices.Devicemaster.lastDiscoveryUpdate;
      if (lastUpdate) {
        const now = new Date().getTime();
        const lastUpdateTime = new Date(lastUpdate).getTime();
        const timeSinceUpdate = now - lastUpdateTime;
        
        // Auto-disable discovery mode after 65 seconds (allowing for some network delay)
        // Only do this if not in physical override mode
        if (timeSinceUpdate > 65000 && !devices.Devicemaster.physicalOverride) {
          updates['Devicemaster/discoveryMode'] = false;
          updates['Devicemaster/lastDiscoveryUpdate'] = new Date().toISOString();
          updatedAny = true;
          console.log('Auto-disabling discovery mode after timeout');
        }
      }
    }
    
    // First check if the master is offline
    let isMasterOnline = false;
    if (devices.Devicemaster) {
      // Check Master's status
      if (devices.Devicemaster.lastHealthPing) {
        const lastPingTime = new Date(devices.Devicemaster.lastHealthPing).getTime();
        const currentTime = new Date().getTime();
        const timeSinceLastPing = currentTime - lastPingTime;
        isMasterOnline = timeSinceLastPing <= MAX_PING_AGE && 
                         (devices.Devicemaster['System Status'] === 'online' || 
                          devices.Devicemaster['System Status'] === undefined);
        
        // Update master's status if needed
        if (devices.Devicemaster.isOnline !== isMasterOnline) {
          updates['Devicemaster/isOnline'] = isMasterOnline;
          updates['Devicemaster/System Status'] = isMasterOnline ? 'online' : 'offline';
          updatedAny = true;
        }
      } else {
        // If no health ping recorded for master, it's offline
        isMasterOnline = false;
        if (devices.Devicemaster.isOnline !== false) {
          updates['Devicemaster/isOnline'] = false;
          updates['Devicemaster/System Status'] = 'offline';
          updatedAny = true;
        }
      }
    }
    
    // Check each device
    for (const deviceKey in devices) {
      if (deviceKey.startsWith('Device') && deviceKey !== 'Devicemaster') {
        const device = devices[deviceKey];
        
        // Initialize lastSeen if it doesn't exist yet
        if (!device.lastSeen) {
          updates[`${deviceKey}/lastSeen`] = device.lastHealthPing || new Date().toISOString();
          updatedAny = true;
        }
        
        // If master is offline, all slaves should be offline
        if (!isMasterOnline) {
          // Set slave devices to offline if master is offline
          if (device.isOnline !== false || device['System Status'] !== 'offline') {
            updates[`${deviceKey}/isOnline`] = false;
            updates[`${deviceKey}/System Status`] = 'offline';
            // Update lastSeen if the device was previously online
            if (device.isOnline === true) {
              const lastSeen = device.lastHealthPing || new Date().toISOString();
              updates[`${deviceKey}/lastSeen`] = lastSeen;
              
              // Calculate time since last update for the UI display
              const lastSeenTime = new Date(lastSeen).getTime();
              const currentTime = new Date().getTime();
              updates[`${deviceKey}/timeSinceUpdateMs`] = currentTime - lastSeenTime;
            }
            updatedAny = true;
          }
          continue;
        }
        
        // If master is online, proceed with normal status checks
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
        const isOnline = timeSinceLastPing <= MAX_PING_AGE && 
                         (device['System Status'] === 'online' || device['System Status'] === undefined);
        
        // Only update if the status has changed or if isOnline property doesn't exist
        if (device.isOnline === undefined || device.isOnline !== isOnline) {
          updates[`${deviceKey}/isOnline`] = isOnline;
          updatedAny = true;
          
          // Also update System Status if needed
          if (device['System Status'] !== (isOnline ? 'online' : 'offline')) {
            updates[`${deviceKey}/System Status`] = isOnline ? 'online' : 'offline';
          }
          
          // If the device is going from online to offline, update the lastSeen timestamp
          if (!isOnline && device.isOnline === true) {
            const lastSeen = device.lastHealthPing || new Date().toISOString();
            updates[`${deviceKey}/lastSeen`] = lastSeen;
            
            // Calculate time since last update for the UI display
            const lastSeenTime = new Date(lastSeen).getTime();
            const currentTime = new Date().getTime();
            updates[`${deviceKey}/timeSinceUpdateMs`] = currentTime - lastSeenTime;
          }
        }
      }
    }
    
    // Apply batch updates if any device status has changed
    if (updatedAny) {
      await db.ref('SParking').update(updates);
      console.log('Updated online status for devices with changes');
      
      // Log successful status updates
      await logApiRequest('system', 'deviceStatus/update', 'SYSTEM', updates, true);
    } else {
      console.log('No device status changes detected');
    }
  } catch (error) {
    console.error('Error updating devices online status:', error);
    
    // Log the error
    await logApiRequest('system', 'deviceStatus/update', 'SYSTEM', {}, false, error.message);
  }
}