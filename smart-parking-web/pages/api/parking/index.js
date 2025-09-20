import db from '../../../lib/firebaseAdmin';
import { validateDeviceId } from '../../../lib/apiHelpers';

export default async function handler(req, res) {
  // Only allow GET requests
  if (req.method !== 'GET') {
    return res.status(405).json({ success: false, error: 'Method not allowed' });
  }

  try {
    // Get all parking spots
    const snapshot = await db.ref('SParking').once('value');
    const data = snapshot.val() || {};
    
    return res.status(200).json({ success: true, data });
  } catch (error) {
    console.error('Error getting parking spots:', error);
    return res.status(500).json({ success: false, error: 'Failed to get parking spots' });
  }
}