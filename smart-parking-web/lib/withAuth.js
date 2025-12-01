import { useEffect, useState, useRef } from 'react';
import { useRouter } from 'next/router';
import { getCurrentUser, refreshSession } from '../lib/auth';

export default function withAuth(Component) {
  return function AuthenticatedComponent(props) {
    const router = useRouter();
    const [loading, setLoading] = useState(true);
    const lastRefreshRef = useRef(Date.now());

    useEffect(() => {
      // Check if the user is authenticated
      const user = getCurrentUser();
      if (!user) {
        // Redirect to login if not authenticated
        router.push('/login');
      } else {
        setLoading(false);
        
        // Refresh session on initial load
        refreshSession();
        lastRefreshRef.current = Date.now();
      }
    }, [router]);

    // Refresh session on user activity (throttled to once per minute)
    useEffect(() => {
      if (!loading) {
        const handleActivity = () => {
          const now = Date.now();
          const timeSinceLastRefresh = now - lastRefreshRef.current;
          
          // Only refresh if it's been more than 1 minute since last refresh
          if (timeSinceLastRefresh > 60000) {
            refreshSession();
            lastRefreshRef.current = now;
          }
        };

        // Refresh session on any user interaction
        window.addEventListener('click', handleActivity);
        window.addEventListener('keypress', handleActivity);

        return () => {
          window.removeEventListener('click', handleActivity);
          window.removeEventListener('keypress', handleActivity);
        };
      }
    }, [loading]);

    // Show a loading state while checking authentication
    if (loading) {
      return (
        <div className="min-h-screen flex items-center justify-center">
          <div className="animate-spin rounded-full h-12 w-12 border-b-2 border-primary-600"></div>
        </div>
      );
    }

    // Render the protected component
    return <Component {...props} />;
  };
}