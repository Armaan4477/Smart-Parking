// Kill Switch API Helper
// This module provides a way to check the kill switch in API routes
import { KILL_SWITCH_ENABLED } from '../context/KillSwitchContext';

/**
 * Check if Firebase communication is disabled by the kill switch
 * @returns {boolean} True if kill switch is enabled (Firebase disabled)
 */
export function isFirebaseDisabled() {
  return KILL_SWITCH_ENABLED;
}

/**
 * Middleware function to check kill switch before allowing Firebase operations
 * Returns early with error response if kill switch is enabled
 * 
 * @param {object} res - Next.js response object
 * @returns {boolean} True if kill switch is enabled (should return early), false otherwise
 */
export function checkKillSwitch(res) {
  if (KILL_SWITCH_ENABLED) {
    res.status(503).json({ 
      success: false, 
      error: 'Firebase communication is currently disabled by kill switch',
      killSwitchEnabled: true
    });
    return true;
  }
  return false;
}

/**
 * Wrapper function for API handlers that need Firebase access
 * Automatically checks kill switch and returns error if enabled
 * 
 * @param {function} handler - The API handler function
 * @returns {function} Wrapped handler function
 */
export function withKillSwitchCheck(handler) {
  return async (req, res) => {
    // Check kill switch before proceeding
    if (checkKillSwitch(res)) {
      return; // Early return if kill switch is enabled
    }
    
    // Proceed with the original handler
    return handler(req, res);
  };
}
