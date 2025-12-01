import { useEffect } from 'react';
import { useRouter } from 'next/router';
import { getCurrentUser } from '../lib/auth';
import Login from '../components/Login';

export default function LoginPage() {
  const router = useRouter();

  useEffect(() => {
    // If user is already logged in, redirect to home page
    const user = getCurrentUser();
    if (user) {
      router.push('/');
    }
  }, [router]);

  return <Login />;
}