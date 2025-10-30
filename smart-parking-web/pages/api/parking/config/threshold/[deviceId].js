import db from '../../../../../lib/firebaseAdmin';
import { validateDeviceId } from '../../../../../lib/apiHelpers';
import { logApiRequest } from '../../../../../lib/logHandler';

export default async function handler(req, res) {
  const { deviceId } = req.query;

  // Validate device ID
  if (!validateDeviceId(deviceId)) {
    return res.status(400).json({ success: false, error: 'Invalid device ID' });
  }

  // Handle GET request (retrieve threshold config for device)
  if (req.method === 'GET') {
    try {
      // Log incoming request
      await logApiRequest(deviceId, `/api/parking/config/threshold/${deviceId}`, 'GET', {}, true);
      
      // First check if there's a device-specific threshold
      const deviceSnapshot = await db.ref(`SConfig/thresholds/Device${deviceId}`).once('value');
      const deviceConfig = deviceSnapshot.val();

      if (deviceConfig && deviceConfig.value !== undefined) {
        return res.status(200).json({ 
          success: true, 
          data: deviceConfig,
          source: 'device' 
        });
      }

      // If no device-specific threshold, get the default
      const defaultSnapshot = await db.ref('SConfig/thresholds/default').once('value');
      const defaultConfig = defaultSnapshot.val();

      if (defaultConfig && defaultConfig.value !== undefined) {
        return res.status(200).json({ 
          success: true, 
          data: defaultConfig,
          source: 'default' 
        });
      }

      // If no configuration at all, return fallback value
      return res.status(200).json({ 
        success: true, 
        data: { value: 45 },
        source: 'fallback'
      });
    } catch (error) {
      console.error(`Error getting threshold config for device ${deviceId}:`, error);
      await logApiRequest(deviceId, `/api/parking/config/threshold/${deviceId}`, 'GET', {}, false, error.message);
      return res.status(500).json({ success: false, error: 'Failed to get threshold configuration' });
    }
  } else {
    return res.status(405).json({ success: false, error: 'Method not allowed' });
  }
}