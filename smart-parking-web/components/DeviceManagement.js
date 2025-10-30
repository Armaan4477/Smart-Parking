import { useState, useEffect } from 'react';

export default function DeviceManagement() {
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [thresholds, setThresholds] = useState({});
  const [defaultThreshold, setDefaultThreshold] = useState('');
  const [updating, setUpdating] = useState(false);
  const [updateMessage, setUpdateMessage] = useState(null);
  const [updateError, setUpdateError] = useState(null);
  
  // Device Mapping states
  const [deviceMappings, setDeviceMappings] = useState([]);
  const [loadingMappings, setLoadingMappings] = useState(true);
  const [mappingsError, setMappingsError] = useState(null);
  const [nextDeviceId, setNextDeviceId] = useState(1);
  const [newMacAddress, setNewMacAddress] = useState('');
  const [newDeviceId, setNewDeviceId] = useState('');
  const [mappingActionInProgress, setMappingActionInProgress] = useState(false);
  const [mappingActionMessage, setMappingActionMessage] = useState(null);
  const [mappingActionError, setMappingActionError] = useState(null);

  // Fetch threshold configurations and device mappings on component mount
  useEffect(() => {
    const fetchThresholds = async () => {
      try {
        setLoading(true);
        const response = await fetch('/api/parking/config/threshold', {
          method: 'GET'
        });
        
        if (!response.ok) {
          throw new Error(`HTTP error! status: ${response.status}`);
        }
        
        const result = await response.json();
        
        if (result.success && result.data) {
          setThresholds(result.data);
          // Set default threshold value for the input field
          if (result.data.default && result.data.default.value) {
            setDefaultThreshold(result.data.default.value.toString());
          }
        }
      } catch (err) {
        console.error('Error fetching threshold configurations:', err);
        setError(err.message || 'Failed to load threshold configurations');
      } finally {
        setLoading(false);
      }
    };
    
    const fetchDeviceMappings = async () => {
      try {
        setLoadingMappings(true);
        const response = await fetch('/api/parking/mappings', {
          method: 'GET'
        });
        
        if (!response.ok) {
          throw new Error(`HTTP error! status: ${response.status}`);
        }
        
        const result = await response.json();
        
        if (result.success) {
          // Sort mappings by deviceId for better readability
          const sortedMappings = result.mappings.sort((a, b) => a.deviceId - b.deviceId);
          setDeviceMappings(sortedMappings);
          setNextDeviceId(result.nextId);
          // Set the default value for the new device ID input
          setNewDeviceId(result.nextId.toString());
        }
      } catch (err) {
        console.error('Error fetching device mappings:', err);
        setMappingsError(err.message || 'Failed to load device mappings');
      } finally {
        setLoadingMappings(false);
      }
    };
    
    fetchThresholds();
    fetchDeviceMappings();
  }, []);
  
  // Handle default threshold update
  const handleUpdateThreshold = async (e) => {
    e.preventDefault();
    
    // Clear previous messages
    setUpdateMessage(null);
    setUpdateError(null);
    
    // Validate input
    const value = parseInt(defaultThreshold, 10);
    if (isNaN(value) || value < 0 || value > 200) {
      setUpdateError('Please enter a valid distance between 0 and 200 cm.');
      return;
    }
    
    try {
      setUpdating(true);
      
      const response = await fetch('/api/parking/config/threshold', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({ value }),
      });
      
      const result = await response.json();
      
      if (result.success) {
        setUpdateMessage(result.message || 'Threshold updated successfully!');
        // Update local state with new value
        setThresholds(prev => ({
          ...prev,
          default: { ...prev.default, value }
        }));
      } else {
        setUpdateError(result.error || 'Failed to update threshold');
      }
    } catch (err) {
      console.error('Error updating threshold:', err);
      setUpdateError(err.message || 'Failed to update threshold');
    } finally {
      setUpdating(false);
      
      // Clear success message after 5 seconds
      if (updateMessage) {
        setTimeout(() => setUpdateMessage(null), 5000);
      }
    }
  };
  
  // Handle adding a new device mapping
  const handleAddMapping = async (e) => {
    e.preventDefault();
    
    // Clear previous messages
    setMappingActionMessage(null);
    setMappingActionError(null);
    
    // Validate MAC address format (simple validation, API will validate more thoroughly)
    if (!newMacAddress || !/^([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2})$/.test(newMacAddress)) {
      setMappingActionError('Please enter a valid MAC address (format: XX:XX:XX:XX:XX:XX).');
      return;
    }
    
    // Validate device ID
    const deviceId = parseInt(newDeviceId, 10);
    if (isNaN(deviceId) || deviceId <= 0) {
      setMappingActionError('Please enter a valid positive device ID.');
      return;
    }
    
    try {
      setMappingActionInProgress(true);
      
      const response = await fetch('/api/parking/mappings/manage', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({ 
          macAddress: newMacAddress,
          deviceId: deviceId
        }),
      });
      
      const result = await response.json();
      
      if (result.success) {
        setMappingActionMessage('Device mapping added successfully!');
        
        // Add the new mapping to the list
        setDeviceMappings(prev => [
          ...prev, 
          {
            macAddress: newMacAddress,
            deviceId: deviceId,
            registeredAt: new Date().toISOString()
          }
        ].sort((a, b) => a.deviceId - b.deviceId));
        
        // Update the next device ID if needed
        if (deviceId >= nextDeviceId) {
          setNextDeviceId(deviceId + 1);
          setNewDeviceId((deviceId + 1).toString());
        }
        
        // Clear the MAC address input
        setNewMacAddress('');
      } else {
        setMappingActionError(result.error || 'Failed to add device mapping');
      }
    } catch (err) {
      console.error('Error adding device mapping:', err);
      setMappingActionError(err.message || 'Failed to add device mapping');
    } finally {
      setMappingActionInProgress(false);
      
      // Clear success message after 5 seconds
      if (mappingActionMessage) {
        setTimeout(() => setMappingActionMessage(null), 5000);
      }
    }
  };
  
  // Handle removing a device mapping
  const handleRemoveMapping = async (macAddress, deviceId) => {
    // Confirm before removing
    if (!confirm(`Are you sure you want to remove the mapping for device ID ${deviceId} (MAC: ${macAddress})?`)) {
      return;
    }
    
    // Clear previous messages
    setMappingActionMessage(null);
    setMappingActionError(null);
    
    try {
      setMappingActionInProgress(true);
      
      const response = await fetch(`/api/parking/mappings/manage?macAddress=${encodeURIComponent(macAddress)}`, {
        method: 'DELETE',
      });
      
      const result = await response.json();
      
      if (result.success) {
        setMappingActionMessage(`Device mapping for ID ${deviceId} removed successfully!`);
        
        // Remove the mapping from the list
        setDeviceMappings(prev => prev.filter(mapping => mapping.macAddress !== macAddress));
      } else {
        setMappingActionError(result.error || 'Failed to remove device mapping');
      }
    } catch (err) {
      console.error('Error removing device mapping:', err);
      setMappingActionError(err.message || 'Failed to remove device mapping');
    } finally {
      setMappingActionInProgress(false);
      
      // Clear success message after 5 seconds
      if (mappingActionMessage) {
        setTimeout(() => setMappingActionMessage(null), 5000);
      }
    }
  };
  
  return (
    <div className="card">
      <h2 className="text-xl font-semibold text-gray-900 mb-6">Device Management</h2>
      
      <div className="mb-8 border-b border-gray-200 pb-8">
        <h3 className="text-lg font-medium text-gray-900 mb-2">Sensor Threshold Configuration</h3>
        <p className="text-sm text-gray-600 mb-4">
          Set the distance threshold (in cm) that determines when a parking spot is considered occupied.
        </p>
        
        {loading ? (
          <div className="flex items-center justify-center py-4 text-gray-500">
            <div className="animate-spin rounded-full h-5 w-5 border-b-2 border-gray-500 mr-3"></div>
            Loading configurations...
          </div>
        ) : error ? (
          <div className="bg-red-50 border border-red-200 rounded-md p-3 text-red-700 mb-4">
            {error}
          </div>
        ) : (
          <div className="bg-gray-50 rounded-lg p-4">
            <form onSubmit={handleUpdateThreshold} className="mb-4">
              <div className="flex flex-col md:flex-row md:items-end gap-3">
                <div className="flex-1">
                  <label htmlFor="threshold" className="block text-sm font-medium text-gray-700 mb-1">
                    Default Threshold (cm):
                  </label>
                  <input
                    id="threshold"
                    type="number"
                    min="0"
                    max="200"
                    className="input"
                    value={defaultThreshold}
                    onChange={(e) => setDefaultThreshold(e.target.value)}
                    placeholder="Distance in cm (0-200)"
                    required
                    disabled={updating}
                  />
                </div>
                <button 
                  type="submit" 
                  className="btn-primary whitespace-nowrap"
                  disabled={updating}
                >
                  {updating ? 'Updating...' : 'Update Threshold'}
                </button>
              </div>
              
              {updateMessage && (
                <div className="mt-3 bg-green-50 border border-green-200 rounded-md p-3 text-green-700">
                  {updateMessage}
                </div>
              )}
              
              {updateError && (
                <div className="mt-3 bg-red-50 border border-red-200 rounded-md p-3 text-red-700">
                  {updateError}
                </div>
              )}
              
              <div className="mt-4 bg-blue-50 border border-blue-200 rounded-md p-3 text-blue-800">
                <p className="flex flex-wrap items-center">
                  <span className="mr-2">Current default threshold:</span> 
                  <span className="font-bold">{thresholds.default?.value || 45} cm</span>
                  {thresholds.default?.updatedAt && (
                    <span className="ml-2 text-xs text-blue-600">
                      (Last updated: {new Date(thresholds.default.updatedAt).toLocaleString()})
                    </span>
                  )}
                </p>
                <p className="text-xs mt-1 text-blue-700">
                  Note: Changes to the threshold will be automatically propagated to all sensors.
                </p>
              </div>
            </form>
          </div>
        )}
      </div>
      
      <div>
        <h3 className="text-lg font-medium text-gray-900 mb-2">Parking Spot Assignments</h3>
        <p className="text-sm text-gray-600 mb-4">
          Manage device ID assignments for parking spots based on MAC addresses.
        </p>
        
        {loadingMappings ? (
          <div className="flex items-center justify-center py-4 text-gray-500">
            <div className="animate-spin rounded-full h-5 w-5 border-b-2 border-gray-500 mr-3"></div>
            Loading device mappings...
          </div>
        ) : mappingsError ? (
          <div className="bg-red-50 border border-red-200 rounded-md p-3 text-red-700 mb-4">
            {mappingsError}
          </div>
        ) : (
          <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
            {/* Add new mapping form */}
                        <div className="bg-gray-50 rounded-lg p-4">
              <h4 className="text-md font-medium text-gray-900 mb-3">Assign New Parking Spot</h4>
              <form onSubmit={handleAddMapping}>
                <div className="mb-4">
                  <label htmlFor="macAddress" className="block text-sm font-medium text-gray-700 mb-1">
                    MAC Address:
                  </label>
                  <input
                    id="macAddress"
                    type="text"
                    className="input"
                    value={newMacAddress}
                    onChange={(e) => setNewMacAddress(e.target.value)}
                    placeholder="XX:XX:XX:XX:XX:XX"
                    required
                    disabled={mappingActionInProgress}
                  />
                </div>
                
                <div className="mb-4">
                  <label htmlFor="deviceId" className="block text-sm font-medium text-gray-700 mb-1">
                    Device ID:
                  </label>
                  <input
                    id="deviceId"
                    type="number"
                    min="1"
                    className="input"
                    value={newDeviceId}
                    onChange={(e) => setNewDeviceId(e.target.value)}
                    placeholder="1, 2, 3, etc."
                    required
                    disabled={mappingActionInProgress}
                  />
                  <p className="text-xs text-gray-500 mt-1">Next available ID: {nextDeviceId}</p>
                </div>
                
                <button 
                  type="submit" 
                  className="btn-primary w-full"
                  disabled={mappingActionInProgress}
                >
                  {mappingActionInProgress ? 'Processing...' : 'Add Mapping'}
                </button>
                
                {mappingActionMessage && (
                  <div className="mt-3 bg-green-50 border border-green-200 rounded-md p-3 text-green-700">
                    {mappingActionMessage}
                  </div>
                )}
                
                {mappingActionError && (
                  <div className="mt-3 bg-red-50 border border-red-200 rounded-md p-3 text-red-700">
                    {mappingActionError}
                  </div>
                )}
            
            </form>
            </div>
            
            {/* Show existing mappings */}
            <div>
              <h4 className="text-md font-medium text-gray-900 mb-3">Current Assignments</h4>
              
              {deviceMappings.length === 0 ? (
                <div className="text-center py-8 bg-gray-50 rounded-lg">
                  <p className="text-gray-500">No device mappings found.</p>
                </div>
              ) : (
                <div className="overflow-hidden bg-white shadow-sm rounded-lg">
                  <table className="min-w-full divide-y divide-gray-200">
                    <thead className="bg-gray-50">
                      <tr>
                        <th scope="col" className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                          Device ID
                        </th>
                        <th scope="col" className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                          MAC Address
                        </th>
                        <th scope="col" className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                          Registered
                        </th>
                        <th scope="col" className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                          Actions
                        </th>
                      </tr>
                    </thead>
                    <tbody className="bg-white divide-y divide-gray-200">
                      {deviceMappings.map((mapping) => (
                        <tr key={mapping.macAddress}>
                          <td className="px-6 py-4 whitespace-nowrap">
                            <span className="px-2 py-1 text-xs font-medium rounded-full bg-primary-100 text-primary-800">
                              {mapping.deviceId}
                            </span>
                          </td>
                          <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-600 font-mono">
                            {mapping.macAddress}
                          </td>
                          <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-500">
                            {new Date(mapping.registeredAt).toLocaleString()}
                          </td>
                          <td className="px-6 py-4 whitespace-nowrap text-sm">
                            <button
                              className="text-red-600 hover:text-red-900"
                              onClick={() => handleRemoveMapping(mapping.macAddress, mapping.deviceId)}
                              disabled={mappingActionInProgress}
                            >
                              Remove
                            </button>
                          </td>
                        </tr>
                      ))}
                    </tbody>
                  </table>
                </div>
              )}
            </div>
          </div>
        )}
      </div>
    </div>
  );
}