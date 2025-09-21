import db from '../../../../lib/firebaseAdmin';
import { validateDeviceId } from '../../../../lib/apiHelpers';
import { logApiRequest } from '../../../../lib/logHandler';

export default async function handler(req, res) {
  // Only allow POST requests for health ping
  if (req.method !== 'POST') {
    return res.status(405).json({ success: false, error: 'Method not allowed' });
  }
  
  // Get device ID from request body
  const { deviceId } = req.body;
  
  // Validate device ID
  if (!validateDeviceId(deviceId)) {
    return res.status(400).json({ success: false, error: 'Invalid device ID' });
  }
  
  try {
    // Log the health ping
    await logApiRequest(deviceId, `/api/parking/health`, 'POST', req.body, true);
    
    // Update the device's last health ping timestamp in the database
    const now = new Date().toISOString();
    const updates = {
      'lastHealthPing': now,
      'isOnline': true
    };
    
    await db.ref(`SParking/Device${deviceId}`).update(updates);
    
    return res.status(200).json({ 
      success: true, 
      message: 'Health ping recorded', 
      timestamp: now 
    });
  } catch (error) {
    console.error('Error updating health ping:', error);
    await logApiRequest(deviceId, `/api/parking/health`, 'POST', req.body, false, error.message);
    return res.status(500).json({ success: false, error: 'Failed to record health ping' });
  }
}