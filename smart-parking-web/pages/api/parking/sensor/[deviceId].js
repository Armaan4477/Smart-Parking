import db from '../../../../lib/firebaseAdmin';
import { validateDeviceId, parseParkingStatus } from '../../../../lib/apiHelpers';

export default async function handler(req, res) {
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
    const { distance, hasSensorError } = req.body;
    
    // Validate distance value
    if (distance === undefined && hasSensorError === undefined) {
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
        return res.status(400).json({ success: false, error: 'Distance must be a number' });
      }
      
      // Parse distance into parking status
      const parkingStatus = parseParkingStatus(distanceValue);
      updates['Parking Status'] = parkingStatus;
      updates['Sensor Error'] = false;
    }
    
    // Always update system status to online when receiving sensor data
    updates['System Status'] = 'online';
    
    // Update database
    await db.ref(`SParking/Device${deviceId}`).update(updates);
    
    return res.status(200).json({ 
      success: true, 
      message: `Sensor data for Device${deviceId} updated successfully`,
      updates
    });
  } catch (error) {
    console.error(`Error updating sensor data for device ${deviceId}:`, error);
    return res.status(500).json({ success: false, error: 'Failed to update sensor data' });
  }
}