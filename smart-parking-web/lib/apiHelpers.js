// Helper functions for API endpoints

/**
 * Helper to validate device ID
 * @param {string|number} deviceId 
 * @returns {boolean}
 */
export const validateDeviceId = (deviceId) => {
  if (!deviceId) return false;
  const id = parseInt(deviceId);
  return !isNaN(id) && id > 0;
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
export const parseParkingStatus = (distance, threshold = 45) => {
  if (distance <= 0) return 'unknown'; // For sensor errors
  return distance < threshold ? 'occupied' : 'open';
};