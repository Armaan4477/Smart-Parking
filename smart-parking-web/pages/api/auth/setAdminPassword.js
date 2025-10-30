import { setAdminPassword } from '../../../lib/auth';
import { isLoggedIn, getCurrentUser } from '../../../lib/auth';
import { logApiRequest } from '../../../lib/logHandler';

/**
 * API endpoint to set the admin password
 */
export default async function handler(req, res) {
  try {
    // Only allow POST method
    if (req.method !== 'POST') {
      return res.status(405).json({ success: false, error: 'Method not allowed' });
    }

    // Check if the request contains the required fields
    const { adminPassword, currentUserPassword } = req.body;
    
    if (!adminPassword || !currentUserPassword) {
      return res.status(400).json({ success: false, error: 'Missing required fields' });
    }

    // Check if user is logged in
    if (!isLoggedIn()) {
      return res.status(401).json({ success: false, error: 'Not authenticated' });
    }

    // Attempt to set admin password
    const success = await setAdminPassword(adminPassword, currentUserPassword);
    
    if (!success) {
      return res.status(400).json({ success: false, error: 'Failed to set admin password' });
    }

    // Return success response
    return res.status(200).json({ success: true, message: 'Admin password set successfully' });
  } catch (error) {
    console.error('Error in setAdminPassword API:', error);
    
    await logApiRequest(
      'system',
      '/api/auth/setAdminPassword',
      'POST',
      { username: getCurrentUser()?.username || 'unknown' },
      false,
      error.message
    );
    
    return res.status(500).json({ success: false, error: 'Internal server error' });
  }
}