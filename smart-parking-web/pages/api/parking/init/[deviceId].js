import db from '../../../../lib/firebaseAdmin';
import { validateDeviceId } from '../../../../lib/apiHelpers';
import { logApiRequest } from '../../../../lib/logHandler';
import { checkKillSwitch } from '../../../../lib/killSwitchApi';

export default async function handler(req, res) {
  // Check kill switch first
  if (checkKillSwitch(res)) {
    return; // Kill switch is enabled, response already sent
  }
  
  // Only allow POST requests for initialization
  if (req.method !== 'POST') {
    return res.status(405).json({ success: false, error: 'Method not allowed' });
  }

  const { deviceId } = req.query;

  // Validate device ID
  if (!validateDeviceId(deviceId)) {
    return res.status(400).json({ success: false, error: 'Invalid device ID' });
  }

  try {
    // Log initialization attempt
    await logApiRequest(deviceId, `/api/parking/init/${deviceId}`, 'POST', req.body, true);
    
    // Check if the device already exists
    const snapshot = await db.ref(`SParking/Device${deviceId}`).once('value');
    
    if (snapshot.exists()) {
      // Device already exists, just return the current data
      return res.status(200).json({ 
        success: true, 
        message: `Device${deviceId} already initialized`,
        data: snapshot.val()
      });
    }
    
    // Initialize device with default values
    await db.ref(`SParking/Device${deviceId}`).set({
      'Parking Status': 'open',
      'Sensor Error': false,
      'System Status': 'online'
    });
    
    return res.status(201).json({ 
      success: true, 
      message: `Device${deviceId} initialized successfully`,
      data: {
        'Parking Status': 'open',
        'Sensor Error': false,
        'System Status': 'online'
      }
    });
  } catch (error) {
    console.error(`Error initializing device ${deviceId}:`, error);
    await logApiRequest(deviceId, `/api/parking/init/${deviceId}`, 'POST', req.body, false, 
      error.message);
    return res.status(500).json({ success: false, error: 'Failed to initialize device' });
  }
}