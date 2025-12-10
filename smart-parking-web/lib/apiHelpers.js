// Helper functions for API endpoints

/**
 * Helper to validate device ID
 * @param {string|number} deviceId 
 * @returns {boolean}
 */
export const validateDeviceId = (deviceId) => {
  if (!deviceId) return false;
  
  // Special case for master ESP32
  if (deviceId === 'master') return true;
  
  // For normal numbered spots
  const id = parseInt(deviceId);
  return !isNaN(id) && id > 0;
};

/**
 * Helper to validate MAC address format
 * @param {string} macAddress 
 * @returns {boolean}
 */
export const validateMacAddress = (macAddress) => {
  if (!macAddress) return false;
  
  // Check if it's a valid MAC address format (e.g., 01:23:45:67:89:AB or 01-23-45-67-89-AB)
  const macRegex = /^([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2})$/;
  return macRegex.test(macAddress);
};

/**
 * Helper to validate parking spot update data
 * @param {object} data 
 * @returns {object} Validation result
 */
export const validateParkingUpdate = (data) => {
  const validatedData = {};
  const errors = [];

  // Check parking status
  if (data.hasOwnProperty('parkingStatus')) {
    if (['open', 'occupied'].includes(data.parkingStatus)) {
      validatedData['Parking Status'] = data.parkingStatus;
    } else {
      errors.push('parkingStatus must be either "open" or "occupied"');
    }
  }

  // Check sensor error
  if (data.hasOwnProperty('sensorError')) {
    if (typeof data.sensorError === 'boolean') {
      validatedData['Sensor Error'] = data.sensorError;
    } else {
      errors.push('sensorError must be a boolean');
    }
  }

  // Check system status
  if (data.hasOwnProperty('systemStatus')) {
    if (['online', 'offline'].includes(data.systemStatus)) {
      validatedData['System Status'] = data.systemStatus;
    } else {
      errors.push('systemStatus must be either "online" or "offline"');
    }
  }

  return {
    isValid: errors.length === 0,
    data: validatedData,
    errors
  };
};

/**
 * Parse distance data into parking status
 * @param {number} distance 
 * @param {number} threshold 
 * @returns {string} 'open' or 'occupied'
 */
export const parseParkingStatus = (distance, threshold = 8) => {
  if (distance <= 0) return 'unknown'; // For sensor errors
  return distance <= threshold ? 'occupied' : 'open';
};