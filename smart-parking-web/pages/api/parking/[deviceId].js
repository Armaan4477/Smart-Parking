import db from '../../../lib/firebaseAdmin';
import { validateDeviceId } from '../../../lib/apiHelpers';
import { logApiRequest } from '../../../lib/logHandler';
import { checkKillSwitch } from '../../../lib/killSwitchApi';

export default async function handler(req, res) {
  // Check kill switch first
  if (checkKillSwitch(res)) {
    return; // Kill switch is enabled, response already sent
  }
  
  const { deviceId } = req.query;

  // Validate device ID
  if (!validateDeviceId(deviceId)) {
    return res.status(400).json({ success: false, error: 'Invalid device ID' });
  }

  // Handle GET request (retrieve device info)
  if (req.method === 'GET') {
    try {
      // Log incoming request
      await logApiRequest(deviceId, `/api/parking/${deviceId}`, 'GET', {}, true);
      
      const snapshot = await db.ref(`SParking/Device${deviceId}`).once('value');
      const data = snapshot.val();

      if (!data) {
        await logApiRequest(deviceId, `/api/parking/${deviceId}`, 'GET', {}, false, 'Device not found');
        return res.status(404).json({ success: false, error: 'Device not found' });
      }

      return res.status(200).json({ success: true, data });
    } catch (error) {
      console.error(`Error getting device ${deviceId}:`, error);
      await logApiRequest(deviceId, `/api/parking/${deviceId}`, 'GET', {}, false, error.message);
      return res.status(500).json({ success: false, error: 'Failed to get device data' });
    }
  } 
  // Handle PUT request (update device info)
  else if (req.method === 'PUT') {
    try {
      const { parkingStatus, sensorError, systemStatus } = req.body;
      
      // Log incoming request
      await logApiRequest(deviceId, `/api/parking/${deviceId}`, 'PUT', req.body, true);
      
      // Build update object with only provided fields
      const updates = {};
      
      if (parkingStatus !== undefined) {
        if (!['open', 'occupied'].includes(parkingStatus)) {
          await logApiRequest(deviceId, `/api/parking/${deviceId}`, 'PUT', req.body, false, 'parkingStatus must be "open" or "occupied"');
          return res.status(400).json({ success: false, error: 'parkingStatus must be "open" or "occupied"' });
        }
        updates['Parking Status'] = parkingStatus;
      }
      
      if (sensorError !== undefined) {
        if (typeof sensorError !== 'boolean') {
          await logApiRequest(deviceId, `/api/parking/${deviceId}`, 'PUT', req.body, false, 'sensorError must be a boolean');
          return res.status(400).json({ success: false, error: 'sensorError must be a boolean' });
        }
        updates['Sensor Error'] = sensorError;
      }
      
      if (systemStatus !== undefined) {
        if (!['online', 'offline'].includes(systemStatus)) {
          await logApiRequest(deviceId, `/api/parking/${deviceId}`, 'PUT', req.body, false, 'systemStatus must be "online" or "offline"');
          return res.status(400).json({ success: false, error: 'systemStatus must be "online" or "offline"' });
        }
        updates['System Status'] = systemStatus;
      }
      
      // Check if any valid updates were provided
      if (Object.keys(updates).length === 0) {
        await logApiRequest(deviceId, `/api/parking/${deviceId}`, 'PUT', req.body, false, 'No valid update fields provided');
        return res.status(400).json({ 
          success: false, 
          error: 'No valid update fields provided' 
        });
      }
      
      // Perform update
      await db.ref(`SParking/Device${deviceId}`).update(updates);
      
      return res.status(200).json({ 
        success: true, 
        message: `Device${deviceId} updated successfully`,
        updates
      });
    } catch (error) {
      console.error(`Error updating device ${deviceId}:`, error);
      await logApiRequest(deviceId, `/api/parking/${deviceId}`, 'PUT', req.body, false, error.message);
      return res.status(500).json({ success: false, error: 'Failed to update device' });
    }
  } 
  // Handle unsupported methods
  else {
    return res.status(405).json({ success: false, error: 'Method not allowed' });
  }
}