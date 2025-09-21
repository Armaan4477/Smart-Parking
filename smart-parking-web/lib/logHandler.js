// Firebase-based log handler for API requests
import { database } from './firebase';
import { ref, push, set, query, orderByChild, limitToLast, get, remove } from 'firebase/database';

// In-memory cache for performance
let apiLogs = [];
const MAX_LOGS = 40;
const LOGS_REF = 'api_logs';

/**
 * Log an API request to Firebase
 * @param {string} deviceId - The device ID making the request
 * @param {string} endpoint - The API endpoint being called
 * @param {string} method - The HTTP method (GET, POST, PUT, etc)
 * @param {object} body - The request body
 * @param {boolean} success - Whether the request was successful
 * @param {string|null} errorMessage - Optional error message if the request failed
 * @returns {object} The log entry
 */
export const logApiRequest = async (deviceId, endpoint, method, body, success, errorMessage = null) => {
  const timestamp = Date.now();
  
  const logEntry = {
    deviceId,
    endpoint,
    method,
    body: typeof body === 'string' ? body : JSON.stringify(body),
    success,
    errorMessage,
    timestamp
  };
  
  try {
    // Add to Firebase
    const newLogRef = push(ref(database, LOGS_REF));
    await set(newLogRef, {
      ...logEntry,
      id: newLogRef.key
    });
    
    // Also update the log entry with the Firebase key
    logEntry.id = newLogRef.key;
    
    // Get current logs count
    const logsCountQuery = query(
      ref(database, LOGS_REF),
      orderByChild('timestamp')
    );
    
    const snapshot = await get(logsCountQuery);
    const totalLogs = snapshot.size;
    
    // If we have more than MAX_LOGS, remove the oldest ones
    if (totalLogs > MAX_LOGS) {
      const oldestLogsQuery = query(
        ref(database, LOGS_REF),
        orderByChild('timestamp'),
        limitToLast(totalLogs - MAX_LOGS)
      );
      
      const oldestLogs = await get(oldestLogsQuery);
      
      // Remove old logs one by one
      oldestLogs.forEach((childSnapshot) => {
        remove(ref(database, `${LOGS_REF}/${childSnapshot.key}`));
      });
    }
    
    // Update the local cache with the updated log
    apiLogs.unshift({...logEntry, id: newLogRef.key});
    
    // Trim local cache to MAX_LOGS
    if (apiLogs.length > MAX_LOGS) {
      apiLogs = apiLogs.slice(0, MAX_LOGS);
    }
    
    // Dispatch event for UI updates if in browser environment
    if (typeof window !== 'undefined') {
      window.dispatchEvent(new CustomEvent('apilog', { 
        detail: { logs: apiLogs } 
      }));
    }
    
    return logEntry;
  } catch (error) {
    console.error('Error logging to Firebase:', error);
    
    // Even if Firebase fails, keep local logs updated
    const localLogEntry = {
      ...logEntry,
      id: Date.now().toString()
    };
    
    apiLogs.unshift(localLogEntry);
    
    if (apiLogs.length > MAX_LOGS) {
      apiLogs = apiLogs.slice(0, MAX_LOGS);
    }
    
    if (typeof window !== 'undefined') {
      window.dispatchEvent(new CustomEvent('apilog', { 
        detail: { logs: apiLogs } 
      }));
    }
    
    return localLogEntry;
  }
};

/**
 * Get all API logs from Firebase
 * @param {number} limit - Maximum number of logs to return
 * @returns {Promise<Array>} Array of log entries
 */
export const getApiLogs = async (limit = MAX_LOGS) => {
  try {
    const logsQuery = query(
      ref(database, LOGS_REF),
      orderByChild('timestamp'),
      limitToLast(limit)
    );
    
    const snapshot = await get(logsQuery);
    const logs = [];
    
    // Firebase returns newest last, so we need to reverse the order
    snapshot.forEach((childSnapshot) => {
      logs.unshift({
        ...childSnapshot.val(),
        id: childSnapshot.key
      });
    });
    
    // Update local cache
    apiLogs = logs;
    
    return logs;
  } catch (error) {
    console.error('Error fetching logs from Firebase:', error);
    return apiLogs; // Return cached logs if Firebase fails
  }
};

/**
 * Get cached API logs (for faster UI updates)
 * @returns {Array} Array of cached log entries
 */
export const getCachedApiLogs = () => {
  return apiLogs;
};