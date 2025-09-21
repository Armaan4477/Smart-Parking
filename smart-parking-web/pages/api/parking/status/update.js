import { updateAllDevicesOnlineStatus } from '../../../lib/deviceStatus';

export default async function handler(req, res) {
  // Only allow GET or POST requests for this endpoint
  if (req.method !== 'GET' && req.method !== 'POST') {
    return res.status(405).json({ success: false, error: 'Method not allowed' });
  }

  try {
    await updateAllDevicesOnlineStatus();
    return res.status(200).json({ success: true, message: 'Devices online status updated successfully' });
  } catch (error) {
    console.error('Error in update devices status endpoint:', error);
    return res.status(500).json({ success: false, error: 'Failed to update devices status' });
  }
}