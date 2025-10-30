import db from '../../../lib/firebaseAdmin';
import { logApiRequest } from '../../../lib/logHandler';

export default async function handler(req, res) {
  // Only allow GET requests for retrieving mappings
  if (req.method !== 'GET') {
    return res.status(405).json({ success: false, error: 'Method not allowed' });
  }

  try {
    // Log the request
    await logApiRequest('system', '/api/parking/mappings', 'GET', {}, true);
    
    // Get all devices
    const devicesRef = db.ref('SParking');
    const snapshot = await devicesRef.once('value');
    const devices = snapshot.val() || {};
    
    let maxDeviceId = 0;
    const mappings = [];
    
    // Extract mapping information from each device
    Object.entries(devices).forEach(([key, value]) => {
      if (key.startsWith('Device') && key !== 'DeviceCount') {
        const deviceIdStr = key.replace('Device', '');
        const deviceId = parseInt(deviceIdStr, 10);
        
        // Skip entries that aren't proper devices or are marked as removed
        if (isNaN(deviceId) || value.removed === true) {
          return;
        }
        
        // Track the highest device ID
        if (deviceId > maxDeviceId) {
          maxDeviceId = deviceId;
        }
        
        // Add to mappings if it has a MAC Address
        if (value['MAC Address']) {
          mappings.push({
            macAddress: value['MAC Address'],
            deviceId: deviceId,
            registeredAt: value.registeredAt || new Date().toISOString()
          });
        }
      }
    });
    
    const nextId = maxDeviceId + 1;
    
    return res.status(200).json({
      success: true,
      mappings,
      nextId
    });
  } catch (error) {
    console.error('Error retrieving device mappings:', error);
    // Log the error
    await logApiRequest('system', '/api/parking/mappings', 'GET', {}, false, error.message);
    return res.status(500).json({ success: false, error: 'Failed to retrieve device mappings' });
  }
}