import { useEffect, useState } from 'react';
import { useRouter } from 'next/router';
import { getCurrentUser } from '../lib/auth';

export default function withAuth(Component) {
  return function AuthenticatedComponent(props) {
    const router = useRouter();
    const [loading, setLoading] = useState(true);

    useEffect(() => {
      // Check if the user is authenticated
      const user = getCurrentUser();
      if (!user) {
        // Redirect to login if not authenticated
        router.push('/login');
      } else {
        setLoading(false);
      }
    }, [router]);

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