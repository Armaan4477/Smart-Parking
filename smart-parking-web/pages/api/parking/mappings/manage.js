import db from '../../../../lib/firebaseAdmin';
import { logApiRequest } from '../../../../lib/logHandler';
import { validateMacAddress } from '../../../../lib/apiHelpers';

export default async function handler(req, res) {
  // Only allow POST for adding and DELETE for removing mappings
  if (req.method !== 'POST' && req.method !== 'DELETE') {
    return res.status(405).json({ success: false, error: 'Method not allowed' });
  }

  try {
    // Add a new mapping
    if (req.method === 'POST') {
      const { macAddress, deviceId } = req.body;

      // Validate required fields
      if (!macAddress || !deviceId) {
        return res.status(400).json({ success: false, error: 'Missing required fields: macAddress or deviceId' });
      }

      // Validate MAC address format
      if (!validateMacAddress(macAddress)) {
        return res.status(400).json({ success: false, error: 'Invalid MAC address format' });
      }

      // Validate deviceId is a number
      const parsedId = parseInt(deviceId, 10);
      if (isNaN(parsedId) || parsedId <= 0) {
        return res.status(400).json({ success: false, error: 'Device ID must be a positive number' });
      }

      // Log the request
      await logApiRequest('system', '/api/parking/mappings/manage', 'POST', req.body, true);

      // Check if any device already has this MAC address
      const devicesRef = db.ref('SParking');
      const devicesSnapshot = await devicesRef.once('value');
      const devices = devicesSnapshot.val() || {};
      
      let macAddressExists = false;
      let deviceIdExists = false;
      
      Object.entries(devices).forEach(([key, value]) => {
        if (key.startsWith('Device') && key !== 'DeviceCount') {
          // Check if MAC address is already registered
          if (value['MAC Address'] === macAddress) {
            macAddressExists = true;
          }
          
          // Check if the device ID is already in use
          if (key === `Device${parsedId}`) {
            deviceIdExists = true;
          }
        }
      });

      if (macAddressExists) {
        return res.status(409).json({ 
          success: false, 
          error: 'MAC address already registered'
        });
      }

      if (deviceIdExists) {
        return res.status(409).json({ 
          success: false, 
          error: 'Device ID already in use'
        });
      }

      // Initialize the device data structure with the mapping info included
      await db.ref(`SParking/Device${parsedId}`).set({
        'Parking Status': 'open',
        'Sensor Error': false,
        'System Status': 'offline',
        'MAC Address': macAddress,
        'registeredAt': new Date().toISOString()
      });

      // Update the device counter if the new ID is higher than the current count
      const countRef = db.ref('SParking/DeviceCount');
      const countSnapshot = await countRef.once('value');
      
      if (!countSnapshot.exists() || countSnapshot.val() < parsedId) {
        await countRef.set(parsedId);
      }

      return res.status(201).json({ 
        success: true, 
        message: 'Device mapping created successfully'
      });
    }

    // Remove an existing mapping
    if (req.method === 'DELETE') {
      const { macAddress } = req.query;

      if (!macAddress) {
        return res.status(400).json({ success: false, error: 'Missing required field: macAddress' });
      }

      // Validate MAC address format
      if (!validateMacAddress(macAddress)) {
        return res.status(400).json({ success: false, error: 'Invalid MAC address format' });
      }

      // Log the request
      await logApiRequest('system', '/api/parking/mappings/manage', 'DELETE', { macAddress }, true);

      // Find the device with this MAC address
      const devicesRef = db.ref('SParking');
      const devicesSnapshot = await devicesRef.once('value');
      const devices = devicesSnapshot.val() || {};
      
      let deviceId = null;
      let deviceKey = null;
      
      Object.entries(devices).forEach(([key, value]) => {
        if (key.startsWith('Device') && key !== 'DeviceCount' && value['MAC Address'] === macAddress) {
          deviceKey = key;
          deviceId = parseInt(key.replace('Device', ''), 10);
        }
      });
      
      if (deviceId === null) {
        return res.status(404).json({ 
          success: false, 
          error: 'Device mapping not found'
        });
      }

      // Optionally remove the device data
      // Commented out to preserve historical data
      // await db.ref(`SParking/Device${deviceId}`).remove();

      // Instead of removing, we'll just update to mark it as removed
      await db.ref(`SParking/${deviceKey}/removed`).set(true);

      return res.status(200).json({ 
        success: true, 
        message: 'Device mapping removed successfully',
        removedDeviceId: deviceId
      });
    }
  } catch (error) {
    console.error('Error managing device mappings:', error);
    // Log the error
    await logApiRequest('system', '/api/parking/mappings/manage', req.method, req.body || req.query, false, error.message);
    return res.status(500).json({ success: false, error: 'Failed to manage device mappings' });
  }
}