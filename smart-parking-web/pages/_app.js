import '../styles/globals.css';
import { useEffect } from 'react';

function MyApp({ Component, pageProps }) {
  // Set up status check on app initialization
  useEffect(() => {
    // Initial status update when app loads
    updateDeviceStatuses();
    
    // Set up interval for periodic status updates
    const interval = setInterval(() => {
      updateDeviceStatuses();
    }, 60000); // Check every minute
    
    return () => clearInterval(interval);
  }, []);
  
  // Function to update device statuses via API
  const updateDeviceStatuses = async () => {
    try {
      await fetch('/api/parking/status/update', {
        method: 'POST',
      });
    } catch (error) {
      console.error('Error updating device statuses:', error);
    }
  };
  
  return <Component {...pageProps} />;
}

export default MyApp;