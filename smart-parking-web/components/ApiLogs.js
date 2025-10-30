import { useState, useEffect } from 'react';
import { getApiLogs, getCachedApiLogs } from '../lib/logHandler';

export default function ApiLogs() {
  const [logs, setLogs] = useState([]);
  const [loading, setLoading] = useState(true);
  const [selectedLog, setSelectedLog] = useState(null);
  const [filter, setFilter] = useState('all');
  const [expanded, setExpanded] = useState(true);

  useEffect(() => {
    // Initialize with cached logs first for fast UI rendering
    setLogs(getCachedApiLogs());
    
    // Then fetch from Firebase
    const fetchLogs = async () => {
      setLoading(true);
      try {
        const firebaseLogs = await getApiLogs();
        setLogs(firebaseLogs);
      } catch (error) {
        console.error('Error fetching logs:', error);
      } finally {
        setLoading(false);
      }
    };
    
    fetchLogs();
    
    const handleLogUpdate = (event) => {
      setLogs([...event.detail.logs]);
    };
    
    window.addEventListener('apilog', handleLogUpdate);
    
    return () => window.removeEventListener('apilog', handleLogUpdate);
  }, []);

  const filteredLogs = logs.filter(log => {
    if (filter === 'all') return true;
    if (filter === 'success') return log.success === true;
    if (filter === 'error') return log.success === false;
    return true;
  });

  const formatTimestamp = (timestamp) => {
    if (!timestamp) return 'Pending...';
    
    try {
      const date = new Date(timestamp);
      return date.toLocaleTimeString() + ' ' + date.toLocaleDateString();
    } catch (e) {
      return 'Invalid timestamp';
    }
  };

  const getEndpointType = (endpoint) => {
    if (endpoint.includes('/init/')) return 'Initialization';
    if (endpoint.includes('/sensor/')) return 'Sensor Update';
    return 'Device Status';
  };

  const getStatusBadge = (success) => {
    return success ? 
      <span className="px-2 py-1 text-xs font-medium rounded-full bg-green-100 text-green-800">Success</span> : 
      <span className="px-2 py-1 text-xs font-medium rounded-full bg-red-100 text-red-800">Error</span>;
  };

  const handleViewDetails = (log) => {
    setSelectedLog(selectedLog === log ? null : log);
  };

  const renderPayloadData = (data) => {
    try {
      const parsed = JSON.parse(data);
      return (
        <pre className="bg-gray-50 p-2 rounded-md text-sm overflow-auto max-h-40 font-mono">
          {JSON.stringify(parsed, null, 2)}
        </pre>
      );
    } catch (e) {
      return <span className="text-gray-600 italic">{data || 'No data'}</span>;
    }
  };

  return (
    <div className="card">
      <div className="flex justify-between items-center mb-4">
        <h2 className="text-xl font-semibold text-gray-900">ESP32 API Request Log</h2>
        <button 
          onClick={() => setExpanded(!expanded)} 
          className="btn-secondary text-sm"
        >
          {expanded ? 'Collapse' : 'Expand'}
        </button>
      </div>

      {expanded && (
        <>
          <div className="flex space-x-2 mb-4">
            <button 
              className={`px-3 py-1 text-sm font-medium rounded-md transition-colors ${
                filter === 'all' 
                  ? 'bg-primary-100 text-primary-800 border border-primary-200' 
                  : 'bg-gray-100 text-gray-700 hover:bg-gray-200'
              }`}
              onClick={() => setFilter('all')}
            >
              All
            </button>
            <button 
              className={`px-3 py-1 text-sm font-medium rounded-md transition-colors ${
                filter === 'success' 
                  ? 'bg-green-100 text-green-800 border border-green-200' 
                  : 'bg-gray-100 text-gray-700 hover:bg-gray-200'
              }`}
              onClick={() => setFilter('success')}
            >
              Success
            </button>
            <button 
              className={`px-3 py-1 text-sm font-medium rounded-md transition-colors ${
                filter === 'error' 
                  ? 'bg-red-100 text-red-800 border border-red-200' 
                  : 'bg-gray-100 text-gray-700 hover:bg-gray-200'
              }`}
              onClick={() => setFilter('error')}
            >
              Errors
            </button>
          </div>

          <div className="space-y-2 max-h-96 overflow-y-auto">
            {loading ? (
              <div className="flex items-center justify-center py-8 text-gray-500">
                <div className="animate-spin rounded-full h-5 w-5 border-b-2 border-gray-500 mr-3"></div>
                Loading API logs from Firebase...
              </div>
            ) : filteredLogs.length > 0 ? (
              filteredLogs.map((log) => (
                <div 
                  key={log.id} 
                  className={`border rounded-lg overflow-hidden ${
                    !log.success ? 'border-red-200 bg-red-50' : 'border-gray-200 bg-white'
                  }`}
                >
                  <div 
                    className="flex items-center justify-between p-3 cursor-pointer hover:bg-gray-50"
                    onClick={() => handleViewDetails(log)}
                  >
                    <div className="flex items-center space-x-4">
                      <div className="w-24 text-xs font-medium text-gray-500 whitespace-nowrap">
                        {getEndpointType(log.endpoint)}
                      </div>
                      <div className="text-sm">
                        <span className="font-medium">Device {log.deviceId}</span>
                        <span className="text-gray-600"> - {log.method} {log.endpoint}</span>
                      </div>
                    </div>
                    <div className="flex items-center space-x-3">
                      <div>
                        {getStatusBadge(log.success)}
                      </div>
                      <div className="text-xs text-gray-500 whitespace-nowrap">
                        {formatTimestamp(log.timestamp)}
                      </div>
                      <div className="text-gray-400">
                        <svg xmlns="http://www.w3.org/2000/svg" className={`h-5 w-5 transition-transform duration-200 ${selectedLog === log ? 'transform rotate-180' : ''}`} fill="none" viewBox="0 0 24 24" stroke="currentColor">
                          <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M19 9l-7 7-7-7" />
                        </svg>
                      </div>
                    </div>
                  </div>
                  
                  {selectedLog === log && (
                    <div className="border-t border-gray-200 p-3 bg-gray-50 space-y-2 text-sm">
                      <div className="grid grid-cols-1 md:grid-cols-2 gap-2">
                        <div>
                          <span className="font-medium">Endpoint:</span> {log.method} {log.endpoint}
                        </div>
                        <div>
                          <span className="font-medium">Device ID:</span> {log.deviceId}
                        </div>
                        <div>
                          <span className="font-medium">Timestamp:</span> {formatTimestamp(log.timestamp)}
                        </div>
                        <div>
                          <span className="font-medium">Status:</span> {log.success ? 'Success' : 'Error'}
                        </div>
                      </div>
                      
                      {!log.success && log.errorMessage && (
                        <div className="mt-2 p-2 bg-red-100 text-red-800 rounded-md">
                          <span className="font-medium">Error:</span> {log.errorMessage}
                        </div>
                      )}
                      
                      <div className="mt-3">
                        <div className="font-medium mb-1">Request Body:</div> 
                        {renderPayloadData(log.body)}
                      </div>
                    </div>
                  )}
                </div>
              ))
            ) : (
              <div className="text-center py-10 text-gray-500">
                <svg xmlns="http://www.w3.org/2000/svg" className="h-12 w-12 mx-auto text-gray-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1} d="M9 12h6m-6 4h6m2 5H7a2 2 0 01-2-2V5a2 2 0 012-2h5.586a1 1 0 01.707.293l5.414 5.414a1 1 0 01.293.707V19a2 2 0 01-2 2z" />
                </svg>
                <p className="mt-2">No API requests logged yet. When ESP32 devices make requests, they will appear here.</p>
              </div>
            )}
          </div>
        </>
      )}
    </div>
  );
}