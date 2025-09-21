import { useState, useEffect } from 'react';
import styles from '../styles/Home.module.css';
import { getApiLogs } from '../lib/logHandler';

export default function ApiLogs() {
  const [logs, setLogs] = useState([]);
  const [selectedLog, setSelectedLog] = useState(null);
  const [filter, setFilter] = useState('all');
  const [expanded, setExpanded] = useState(true);

  useEffect(() => {
    setLogs(getApiLogs());
    
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
      <span className={styles.successBadge}>Success</span> : 
      <span className={styles.errorBadge}>Error</span>;
  };

  const handleViewDetails = (log) => {
    setSelectedLog(selectedLog === log ? null : log);
  };

  const renderPayloadData = (data) => {
    try {
      const parsed = JSON.parse(data);
      return (
        <pre className={styles.jsonData}>
          {JSON.stringify(parsed, null, 2)}
        </pre>
      );
    } catch (e) {
      return <span>{data || 'No data'}</span>;
    }
  };

  return (
    <div className={styles.apiLogsContainer}>
      <div className={styles.apiLogsHeader}>
        <h2>ESP32 API Request Log</h2>
        <button 
          onClick={() => setExpanded(!expanded)} 
          className={styles.toggleButton}
        >
          {expanded ? 'Collapse' : 'Expand'}
        </button>
      </div>

      {expanded && (
        <>
          <div className={styles.apiLogsFilters}>
            <button 
              className={`${styles.filterButton} ${filter === 'all' ? styles.activeFilter : ''}`}
              onClick={() => setFilter('all')}
            >
              All
            </button>
            <button 
              className={`${styles.filterButton} ${filter === 'success' ? styles.activeFilter : ''}`}
              onClick={() => setFilter('success')}
            >
              Success
            </button>
            <button 
              className={`${styles.filterButton} ${filter === 'error' ? styles.activeFilter : ''}`}
              onClick={() => setFilter('error')}
            >
              Errors
            </button>
          </div>

          <div className={styles.apiLogsList}>
            {filteredLogs.length > 0 ? (
              filteredLogs.map((log) => (
                <div 
                  key={log.id} 
                  className={`${styles.apiLogItem} ${!log.success ? styles.errorLog : ''}`}
                >
                  <div className={styles.apiLogMain} onClick={() => handleViewDetails(log)}>
                    <div className={styles.apiLogType}>
                      {getEndpointType(log.endpoint)}
                    </div>
                    <div className={styles.apiLogInfo}>
                      <strong>Device {log.deviceId}</strong> - {log.method} {log.endpoint}
                    </div>
                    <div className={styles.apiLogStatus}>
                      {getStatusBadge(log.success)}
                    </div>
                    <div className={styles.apiLogTime}>
                      {formatTimestamp(log.timestamp)}
                    </div>
                  </div>
                  
                  {selectedLog === log && (
                    <div className={styles.apiLogDetails}>
                      <div className={styles.apiLogDetail}>
                        <strong>Endpoint:</strong> {log.method} {log.endpoint}
                      </div>
                      <div className={styles.apiLogDetail}>
                        <strong>Device ID:</strong> {log.deviceId}
                      </div>
                      <div className={styles.apiLogDetail}>
                        <strong>Timestamp:</strong> {formatTimestamp(log.timestamp)}
                      </div>
                      <div className={styles.apiLogDetail}>
                        <strong>Status:</strong> {log.success ? 'Success' : 'Error'}
                      </div>
                      {!log.success && log.errorMessage && (
                        <div className={styles.apiLogDetail}>
                          <strong>Error:</strong> {log.errorMessage}
                        </div>
                      )}
                      <div className={styles.apiLogDetail}>
                        <strong>Request Body:</strong> 
                        {renderPayloadData(log.body)}
                      </div>
                    </div>
                  )}
                </div>
              ))
            ) : (
              <div className={styles.noLogsMessage}>
                No API requests logged yet. When ESP32 devices make requests, they will appear here.
              </div>
            )}
          </div>
        </>
      )}
    </div>
  );
}