import db from '../../../../lib/firebaseAdmin';
import { logApiRequest } from '../../../../lib/logHandler';
import { validateDeviceId } from '../../../../lib/apiHelpers';

export default async function handler(req, res) {
  // Handle GET request for fetching all thresholds
  if (req.method === 'GET') {
    try {
      // Log incoming request
      await logApiRequest('system', '/api/parking/config/threshold', 'GET', {}, true);
      
      const snapshot = await db.ref('SConfig/thresholds').once('value');
      const data = snapshot.val() || {};
      
      if (!data.default) {
        // Initialize default threshold if not present
        await db.ref('SConfig/thresholds/default').set({
          value: 45,
          updatedAt: new Date().toISOString()
        });
        
        data.default = { value: 45, updatedAt: new Date().toISOString() };
      }
      
      return res.status(200).json({ success: true, data });
    } catch (error) {
      console.error('Error fetching threshold configurations:', error);
      await logApiRequest('system', '/api/parking/config/threshold', 'GET', {}, false, error.message);
      return res.status(500).json({ success: false, error: 'Failed to fetch threshold configurations' });
    }
  }
  
  // Handle POST method for updating thresholds
  if (req.method === 'POST') {
    // Extract data from the request
    const { value, deviceId = null } = req.body;

    // Validate threshold value
    if (value === undefined || value === null || isNaN(value)) {
      return res.status(400).json({ success: false, error: 'Missing or invalid threshold value' });
    }

    const numValue = parseInt(value, 10);
    if (numValue < 0 || numValue > 200) {
      return res.status(400).json({ 
        success: false, 
        error: 'Threshold value must be between 0 and 200 cm' 
      });
    }

    try {
      let path = 'SConfig/thresholds/default';
      let logPath = '/api/parking/config/threshold';
      
      // If deviceId is provided, update device-specific threshold
      if (deviceId) {
        // Validate device ID format
        if (!validateDeviceId(deviceId)) {
          return res.status(400).json({ success: false, error: 'Invalid device ID format' });
        }
        
        path = `SConfig/thresholds/Device${deviceId}`;
        logPath = `/api/parking/config/threshold/${deviceId}`;
      }

      // Log incoming request
      await logApiRequest('system', logPath, 'POST', req.body, true);

      // Update the threshold in database
      const updates = {
        value: numValue,
        updatedAt: new Date().toISOString()
      };

      await db.ref(path).update(updates);

      // If updating default threshold, also set a flag to notify the master to update all slaves
      if (!deviceId) {
        await db.ref('SConfig/thresholds/updateRequired').set(true);
      }

      return res.status(200).json({ 
        success: true, 
        message: deviceId 
          ? `Threshold updated for device ${deviceId} to ${numValue} cm` 
          : `Default threshold updated to ${numValue} cm`,
        data: updates
      });
    } catch (error) {
      console.error('Error updating threshold:', error);
      await logApiRequest('system', '/api/parking/config/threshold', 'POST', req.body, false, error.message);
      return res.status(500).json({ success: false, error: 'Failed to update threshold configuration' });
    }
  }
  
  // Handle other HTTP methods
  return res.status(405).json({ success: false, error: 'Method not allowed' });
}
