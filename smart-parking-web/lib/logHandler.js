// In-memory log handler for API requests
// This replaces the Firebase database logging functionality

// Store logs in memory
let apiLogs = [];
const MAX_LOGS = 30;

/**
 * Log an API request to in-memory storage
 * @param {string} deviceId - The device ID making the request
 * @param {string} endpoint - The API endpoint being called
 * @param {string} method - The HTTP method (GET, POST, PUT, etc)
 * @param {object} body - The request body
 * @param {boolean} success - Whether the request was successful
 * @param {string|null} errorMessage - Optional error message if the request failed
 * @returns {object} The log entry
 */
export const logApiRequest = (deviceId, endpoint, method, body, success, errorMessage = null) => {
  const logEntry = {
    id: Date.now().toString(),
    deviceId,
    endpoint,
    method,
    body: typeof body === 'string' ? body : JSON.stringify(body),
    success,
    errorMessage,
    timestamp: Date.now()
  };
  
  // Add to the beginning of the array (most recent first)
  apiLogs.unshift(logEntry);
  
  // Limit array size to prevent memory issues
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
};

/**
 * Get all API logs
 * @param {number} limit - Maximum number of logs to return
 * @returns {Array} Array of log entries
 */
export const getApiLogs = (limit = MAX_LOGS) => {
  return apiLogs.slice(0, limit);
};