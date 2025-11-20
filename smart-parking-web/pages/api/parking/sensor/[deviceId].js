import db from '../../../../lib/firebaseAdmin';
import { validateDeviceId, parseParkingStatus } from '../../../../lib/apiHelpers';
import { logApiRequest } from '../../../../lib/logHandler';
import { checkKillSwitch } from '../../../../lib/killSwitchApi';

export default async function handler(req, res) {
  // Check kill switch first
  if (checkKillSwitch(res)) {
    return; // Kill switch is enabled, response already sent
  }
  
  // Only allow POST requests for sensor updates
  if (req.method !== 'POST') {
    return res.status(405).json({ success: false, error: 'Method not allowed' });
  }

  const { deviceId } = req.query;

  // Validate device ID
  if (!validateDeviceId(deviceId)) {
    return res.status(400).json({ success: false, error: 'Invalid device ID' });
  }

  try {
    // Log incoming sensor request
    await logApiRequest(deviceId, `/api/parking/sensor/${deviceId}`, 'POST', req.body, true);
    
    const { distance, hasSensorError, isOffline, lastUpdateMs, timeSinceUpdateMs } = req.body;
    
    // Validate distance value
    if (distance === undefined && hasSensorError === undefined) {
      await logApiRequest(deviceId, `/api/parking/sensor/${deviceId}`, 'POST', req.body, false, 
        'Either distance or hasSensorError must be provided');
      return res.status(400).json({ 
        success: false, 
        error: 'Either distance or hasSensorError must be provided' 
      });
    }
    
    const updates = {};
    
    // Handle sensor error case
    if (hasSensorError === true) {
      updates['Sensor Error'] = true;
      updates['Parking Status'] = 'unknown'; // Can't determine status with sensor error
    } 
    // Handle normal distance reading
    else if (distance !== undefined) {
      const distanceValue = parseInt(distance);
      
      if (isNaN(distanceValue)) {
        await logApiRequest(deviceId, `/api/parking/sensor/${deviceId}`, 'POST', req.body, false, 
          'Distance must be a number');
        return res.status(400).json({ success: false, error: 'Distance must be a number' });
      }
      
      // Parse distance into parking status
      const parkingStatus = parseParkingStatus(distanceValue);
      updates['Parking Status'] = parkingStatus;
      updates['Sensor Error'] = false;
    }
    
    // Update system status based on offline flag if provided, otherwise default to online
    if (isOffline !== undefined) {
      updates['System Status'] = isOffline ? 'offline' : 'online';
      
      // Store additional timestamp information if provided
      if (lastUpdateMs !== undefined) {
        updates['lastUpdateMs'] = lastUpdateMs;
      }
      
      if (timeSinceUpdateMs !== undefined) {
        updates['timeSinceUpdateMs'] = timeSinceUpdateMs;
      }
    } else {
      updates['System Status'] = 'online';
    }
    
    // Update database
    await db.ref(`SParking/Device${deviceId}`).update(updates);
    
    return res.status(200).json({ 
      success: true, 
      message: `Sensor data for Device${deviceId} updated successfully`,
      updates
    });
  } catch (error) {
    console.error(`Error updating sensor data for device ${deviceId}:`, error);
    await logApiRequest(deviceId, `/api/parking/sensor/${deviceId}`, 'POST', req.body, false, 
      error.message);
    return res.status(500).json({ success: false, error: 'Failed to update sensor data' });
  }
}