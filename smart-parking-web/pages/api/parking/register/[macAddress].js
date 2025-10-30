import db from '../../../../lib/firebaseAdmin';
import { logApiRequest } from '../../../../lib/logHandler';
import { validateMacAddress } from '../../../../lib/apiHelpers';

/**
 * API handler for registering devices by MAC address
 * This endpoint will:
 * 1. Check if the MAC address is already registered
 * 2. If yes, return the existing ID
 * 3. If no, assign a new ID and store the mapping within the device object itself
 */
export default async function handler(req, res) {
  // Only allow POST requests for device registration
  if (req.method !== 'POST') {
    return res.status(405).json({ success: false, error: 'Method not allowed' });
  }

  // Extract MAC address from the URL
  const { macAddress } = req.query;

  // Validate the MAC address
  if (!validateMacAddress(macAddress)) {
    return res.status(400).json({ success: false, error: 'Invalid MAC address format' });
  }

  try {
    // Log registration attempt
    await logApiRequest('register', `/api/parking/register/${macAddress}`, 'POST', req.body, true);
    
    // Check if any device already has this MAC address
    const devicesRef = db.ref('SParking');
    const devicesSnapshot = await devicesRef.once('value');
    const devices = devicesSnapshot.val() || {};
    
    // Look for the MAC address in all Device entries
    let existingDeviceId = null;
    let maxDeviceId = 0;
    
    Object.entries(devices).forEach(([key, value]) => {
      if (key.startsWith('Device') && key !== 'DeviceCount') {
        const deviceIdStr = key.replace('Device', '');
        const deviceId = parseInt(deviceIdStr, 10);
        
        // Track the highest device ID we've seen
        if (!isNaN(deviceId) && deviceId > maxDeviceId) {
          maxDeviceId = deviceId;
        }
        
        // Check if this device has our MAC address
        if (value['MAC Address'] === macAddress) {
          existingDeviceId = deviceId;
        }
      }
    });
    
    if (existingDeviceId !== null) {
      // Device is already registered, return existing ID
      return res.status(200).json({ 
        success: true, 
        message: 'Device already registered',
        isNewDevice: false,
        deviceId: existingDeviceId
      });
    }
    
    // This is a new device, we need to assign an ID
    const nextId = maxDeviceId + 1;
    
    // Initialize the device data structure with mapping info included
    await db.ref(`SParking/Device${nextId}`).set({
      'Parking Status': 'open',
      'Sensor Error': false,
      'System Status': 'online',
      'MAC Address': macAddress,
      'registeredAt': new Date().toISOString()
    });
    
    // Update the device counter (still keep this for compatibility)
    await db.ref('SParking/DeviceCount').set(nextId);
    
    return res.status(201).json({ 
      success: true, 
      message: 'Device registered successfully',
      isNewDevice: true,
      deviceId: nextId
    });
  } catch (error) {
    console.error('Error in device registration:', error);
    // Log the error
    await logApiRequest('register', `/api/parking/register/${macAddress}`, 'POST', req.body, false, error.message);
    return res.status(500).json({ success: false, error: 'Failed to register device' });
  }
}