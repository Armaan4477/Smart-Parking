import { useState, useEffect } from 'react';
import Head from 'next/head';
import styles from '../styles/Home.module.css';
import ApiLogs from '../components/ApiLogs';
import { subscribeToAllParkingSpots, unsubscribeFromParkingSpots } from '../lib/database';

export default function Home() {
  const [parkingSpots, setParkingSpots] = useState({});
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [updatedSpots, setUpdatedSpots] = useState({});

  // Status updates are now handled in _app.js

  useEffect(() => {
    // Initialize with loading state
    setLoading(true);
    setError(null);
    
    // Set up real-time subscription
    let spotsRef;
    try {
      spotsRef = subscribeToAllParkingSpots((result) => {
        if (result.success) {
          // Track which spots have been updated for visual feedback
          const newSpots = result.data || {};
          
          setParkingSpots(prevSpots => {
            // Find spots that have changed status
            const changedSpots = {};
            Object.entries(newSpots).forEach(([key, spot]) => {
              if (prevSpots[key] && prevSpots[key]['Parking Status'] !== spot['Parking Status']) {
                changedSpots[key] = true;
              }
            });
            
            // Remember updated spots for visual feedback
            if (Object.keys(changedSpots).length > 0) {
              setUpdatedSpots(changedSpots);
              // Clear the highlight after 2 seconds
              setTimeout(() => {
                setUpdatedSpots({});
              }, 2000);
            }
            
            return newSpots;
          });
        } else {
          console.error('Error in real-time parking spots update:', result.error);
          setError(result.error || 'Failed to load parking spots');
        }
        
        // Set loading to false once we receive the first data
        setLoading(false);
      });
    } catch (err) {
      console.error('Error setting up real-time subscription:', err);
      setError(err.message || 'Failed to set up real-time updates');
      setLoading(false);
    }
    
    // Cleanup function to unsubscribe when component unmounts
    return () => {
      if (spotsRef) {
        unsubscribeFromParkingSpots(spotsRef);
      }
    };
  }, []);

  return (
    <div className={styles.container}>
      <Head>
        <title>Smart Parking System</title>
        <meta name="description" content="Smart Parking System API and Dashboard" />
        <link rel="icon" href="/favicon.ico" sizes="any" />
        <link rel="apple-touch-icon" href="/apple-touch-icon.png" />
        <link rel="apple-touch-icon-precomposed" href="/apple-touch-icon.png" />
        <link rel="manifest" href="/site.webmanifest" />
      </Head>

      <main className={styles.main}>
        <div className={styles.titleContainer}>
          <img src="/ouricon.png" alt="Smart Parking Icon" className={styles.logo} />
          <h1 className={styles.title}>
            Welcome to Smart Parking System
          </h1>
        </div>

        <p className={styles.description}>
          API status: Active
        </p>
        
        <div className={styles.masterStatus}>
          {parkingSpots.Devicemaster ? (
            <div className={parkingSpots.Devicemaster.isOnline ? styles.masterOnline : styles.masterOffline}>
              <div className={styles.statusIcon}></div>
              <span>Master ESP32: {parkingSpots.Devicemaster.isOnline ? 'Online' : 'Offline - Not Reachable'}</span>
            </div>
          ) : (
            <div className={styles.masterOffline}>
              <div className={styles.statusIcon}></div>
              <span>Master ESP32: Not Connected</span>
            </div>
          )}
        </div>
        
        <div className={styles.dashboard}>
          <div className={styles.parkingStatus}>
            <h2>Parking Spots Status</h2>
            {error ? (
              <div className={styles.error}>
                <p>Error loading parking spots: {error}</p>
                <button 
                  onClick={() => window.location.reload()}
                  className={styles.retryButton}
                >
                  Retry
                </button>
              </div>
            ) : loading ? (
              <div className={styles.loading}>
                <p>Loading parking data...</p>
              </div>
            ) : Object.keys(parkingSpots).filter(key => key !== 'Devicemaster').length > 0 ? (
              <div className={styles.spotsGrid}>
                {Object.entries(parkingSpots).filter(([key]) => key !== 'Devicemaster').map(([key, spot]) => {
                  const deviceId = key.replace('Device', '');
                  const isUpdated = updatedSpots[key];
                  return (
                    <div 
                      key={deviceId} 
                      className={`${styles.spotCard} 
                                ${spot['Parking Status'] === 'occupied' ? styles.occupied : styles.available} 
                                ${spot['Sensor Error'] ? styles.error : ''}
                                ${isUpdated ? styles.updated : ''}`}
                    >
                      <h3>Spot {deviceId}</h3>
                      <p>Status: {spot['Parking Status']}</p>
                      <p>System: {spot['System Status'] || 'unknown'}</p>
                      {spot['Sensor Error'] && (
                        <p className={styles.errorText}>Sensor Error</p>
                      )}
                      {isUpdated && <div className={styles.updateIndicator}>Updated</div>}
                    </div>
                  );
                })}
              </div>
            ) : (
              <p>No parking spots registered yet.</p>
            )}
          </div>
          
          {/* API Request Logs Section */}
          <ApiLogs />
        </div>
      </main>

      <footer className={styles.footer}>
        <p>Smart Parking System - ESP32 Firebase Integration</p>
      </footer>
    </div>
  );
}