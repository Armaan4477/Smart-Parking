import { updateAllDevicesOnlineStatus, updateOfflineDeviceTimers } from '../../../../lib/deviceStatus';
import { logApiRequest } from '../../../../lib/logHandler';
import { checkKillSwitch } from '../../../../lib/killSwitchApi';

export default async function handler(req, res) {
  // Check kill switch first
  if (checkKillSwitch(res)) {
    return; // Kill switch is enabled, response already sent
  }
  
  // Only allow GET or POST requests for this endpoint
  if (req.method !== 'GET' && req.method !== 'POST') {
    return res.status(405).json({ success: false, error: 'Method not allowed' });
  }

  try {
    // Log the status update request - use 'system' as deviceId since this updates all devices
    await logApiRequest('system', '/api/parking/status/update', req.method, {}, true);
    
    // First update online status for all devices
    await updateAllDevicesOnlineStatus();
    
    // Then update timers for offline devices
    await updateOfflineDeviceTimers();
    
    return res.status(200).json({ success: true, message: 'Devices online status updated successfully' });
  } catch (error) {
    console.error('Error in update devices status endpoint:', error);
    // Log the error
    await logApiRequest('system', '/api/parking/status/update', req.method, {}, false, error.message);
    return res.status(500).json({ success: false, error: 'Failed to update devices status' });
  }
}