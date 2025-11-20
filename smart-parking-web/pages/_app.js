import '../styles/globals.css';
import { useEffect, useState } from 'react';
import { useRouter } from 'next/router';
import { isLoggedIn } from '../lib/auth';
import { KillSwitchProvider } from '../context/KillSwitchContext';

function MyApp({ Component, pageProps }) {
  const router = useRouter();
  const [authChecked, setAuthChecked] = useState(false);

  // Check authentication on initial load and route changes
  useEffect(() => {
    const checkAuth = () => {
      // Skip auth check for login page
      if (router.pathname === '/login') {
        setAuthChecked(true);
        return;
      }

      // Check if user is logged in
      if (!isLoggedIn()) {
        // Redirect to login if not on login page
        router.push('/login');
      } else {
        setAuthChecked(true);
      }
    };

    checkAuth();
    router.events.on('routeChangeComplete', checkAuth);
    
    return () => {
      router.events.off('routeChangeComplete', checkAuth);
    };
  }, [router]);

  // Set up status check on app initialization
  useEffect(() => {
    // Initial status update when app loads
    updateDeviceStatuses();
    
    // Set up interval for periodic status updates
    const interval = setInterval(() => {
      updateDeviceStatuses();
    }, 30000); // Check every 30 seconds to detect offline status more quickly
    
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
  
  // Don't render component until authentication is checked
  // Except for the login page which should always render
  if (!authChecked && router.pathname !== '/login') {
    return <div>Loading...</div>;
  }
  
  return (
    <KillSwitchProvider>
      <Component {...pageProps} />
    </KillSwitchProvider>
  );
}

export default MyApp;