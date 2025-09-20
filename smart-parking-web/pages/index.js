import { useState, useEffect } from 'react';
import Head from 'next/head';
import styles from '../styles/Home.module.css';
import ApiLogs from '../components/ApiLogs';
import { getAllParkingSpots } from '../lib/database';

export default function Home() {
  const [parkingSpots, setParkingSpots] = useState({});
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    async function fetchParkingSpots() {
      try {
        const result = await getAllParkingSpots();
        if (result.success) {
          setParkingSpots(result.data || {});
        }
      } catch (error) {
        console.error('Error fetching parking spots:', error);
      } finally {
        setLoading(false);
      }
    }

    fetchParkingSpots();
    const interval = setInterval(fetchParkingSpots, 10000); // Refresh every 10 seconds
    
    return () => clearInterval(interval);
  }, []);

  return (
    <div className={styles.container}>
      <Head>
        <title>Smart Parking System</title>
        <meta name="description" content="Smart Parking System API and Dashboard" />
        <link rel="icon" href="/favicon.ico" />
      </Head>

      <main className={styles.main}>
        <h1 className={styles.title}>
          Welcome to Smart Parking System
        </h1>

        <p className={styles.description}>
          API status: Active
        </p>
        
        <div className={styles.dashboard}>
          <div className={styles.parkingStatus}>
            <h2>Parking Spots Status</h2>
            {loading ? (
              <p>Loading parking data...</p>
            ) : Object.keys(parkingSpots).length > 0 ? (
              <div className={styles.spotsGrid}>
                {Object.entries(parkingSpots).map(([key, spot]) => {
                  const deviceId = key.replace('Device', '');
                  return (
                    <div 
                      key={deviceId} 
                      className={`${styles.spotCard} ${spot['Parking Status'] === 'occupied' ? styles.occupied : styles.available} 
                                ${spot['Sensor Error'] ? styles.error : ''}`}
                    >
                      <h3>Spot {deviceId}</h3>
                      <p>Status: {spot['Parking Status']}</p>
                      <p>System: {spot['System Status'] || 'unknown'}</p>
                      {spot['Sensor Error'] && (
                        <p className={styles.errorText}>Sensor Error</p>
                      )}
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