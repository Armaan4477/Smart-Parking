import { database } from '../../../lib/firebase';
import { ref, get } from 'firebase/database';
import { logApiRequest } from '../../../lib/logHandler';

/**
 * API endpoint to check if admin password is set
 */
export default async function handler(req, res) {
  try {
    // Only allow GET method
    if (req.method !== 'GET') {
      return res.status(405).json({ success: false, error: 'Method not allowed' });
    }

    // Check if admin password is set
    const adminConfigRef = ref(database, 'adminConfig');
    const adminConfigSnapshot = await get(adminConfigRef);
    
    const adminPasswordSet = adminConfigSnapshot.exists() && 
                             adminConfigSnapshot.val().adminPasswordHash && 
                             adminConfigSnapshot.val().adminSalt;

    // Log the API request (for debugging)
    await logApiRequest(
      'system',
      '/api/auth/checkAdminConfig',
      'GET',
      {},
      true,
      `Admin password is ${adminPasswordSet ? 'set' : 'not set'}`
    );

    // Return response
    return res.status(200).json({ 
      success: true, 
      adminPasswordSet,
      isFirstRegistration: !adminPasswordSet 
    });
  } catch (error) {
    console.error('Error checking admin config:', error);
    
    await logApiRequest(
      'system',
      '/api/auth/checkAdminConfig',
      'GET',
      {},
      false,
      error.message
    );
    
    return res.status(500).json({ success: false, error: 'Internal server error' });
  }
}