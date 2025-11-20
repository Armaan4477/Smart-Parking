import db from '../../../lib/firebaseAdmin';
import { validateDeviceId } from '../../../lib/apiHelpers';
import { logApiRequest } from '../../../lib/logHandler';
import { checkKillSwitch } from '../../../lib/killSwitchApi';

export default async function handler(req, res) {
  // Check kill switch first
  if (checkKillSwitch(res)) {
    return; // Kill switch is enabled, response already sent
  }
  
  // Only allow GET requests
  if (req.method !== 'GET') {
    return res.status(405).json({ success: false, error: 'Method not allowed' });
  }

  try {
    // Log the request to get all parking spots
    await logApiRequest('all', '/api/parking', 'GET', {}, true);
    
    // Get all parking spots
    const snapshot = await db.ref('SParking').once('value');
    const data = snapshot.val() || {};
    
    return res.status(200).json({ success: true, data });
  } catch (error) {
    console.error('Error getting parking spots:', error);
    // Log the error
    await logApiRequest('all', '/api/parking', 'GET', {}, false, error.message);
    return res.status(500).json({ success: false, error: 'Failed to get parking spots' });
  }
}