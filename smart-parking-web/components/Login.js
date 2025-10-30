import { useState } from 'react';
import { useRouter } from 'next/router';
import { login, register } from '../lib/auth';

export default function Login() {
  const [username, setUsername] = useState('');
  const [password, setPassword] = useState('');
  const [adminPassword, setAdminPassword] = useState('');
  const [isLogin, setIsLogin] = useState(true);
  const [error, setError] = useState(null);
  const [loading, setLoading] = useState(false);
  const router = useRouter();

  const handleSubmit = async (e) => {
    e.preventDefault();
    setError(null);
    setLoading(true);
    
    try {
      if (isLogin) {
        // Handle login
        const success = await login(username, password);
        if (success) {
          router.push('/');
        } else {
          setError('Invalid username or password');
        }
      } else {
        // Handle registration
        const success = await register(username, password, adminPassword);
        if (success) {
          // Switch to login mode after successful registration
          setIsLogin(true);
          setError(null);
          setUsername('');
          setPassword('');
          setAdminPassword('');
          // Add a success message indicating successful registration
          setError('Registration successful! Please log in with your new credentials.');
        } else {
          setError('Registration failed. Please check your admin password or try a different username.');
        }
      }
    } catch (err) {
      console.error('Authentication error:', err);
      setError(err.message || 'Authentication failed');
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="min-h-screen flex items-center justify-center bg-gray-50 py-12 px-4 sm:px-6 lg:px-8">
      <div className="max-w-md w-full space-y-8 bg-white p-8 rounded-xl shadow-lg">
        <div className="text-center">
          <img src="/ouricon.png" alt="Smart Parking Logo" className="h-20 w-20 mx-auto" />
          <h1 className="mt-4 text-3xl font-extrabold text-gray-900">Smart Parking System</h1>
          <h2 className="mt-1 text-xl font-medium text-gray-700">
            {isLogin ? 'Sign in to your account' : 'Create a new account'}
          </h2>
        </div>
        
        {error && (
          <div className={`rounded-md p-4 ${error.includes('successful') ? 'bg-green-50' : 'bg-red-50'}`}>
            <div className="flex">
              <div className="shrink-0">
                {error.includes('successful') ? (
                  <svg className="h-5 w-5 text-green-400" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 20 20" fill="currentColor">
                    <path fillRule="evenodd" d="M10 18a8 8 0 100-16 8 8 0 000 16zm3.707-9.293a1 1 0 00-1.414-1.414L9 10.586 7.707 9.293a1 1 0 00-1.414 1.414l2 2a1 1 0 001.414 0l4-4z" clipRule="evenodd" />
                  </svg>
                ) : (
                  <svg className="h-5 w-5 text-red-400" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 20 20" fill="currentColor">
                    <path fillRule="evenodd" d="M10 18a8 8 0 100-16 8 8 0 000 16zM8.707 7.293a1 1 0 00-1.414 1.414L8.586 10l-1.293 1.293a1 1 0 101.414 1.414L10 11.414l1.293 1.293a1 1 0 001.414-1.414L11.414 10l1.293-1.293a1 1 0 00-1.414-1.414L10 8.586 8.707 7.293z" clipRule="evenodd" />
                  </svg>
                )}
              </div>
              <div className="ml-3">
                <p className={`text-sm ${error.includes('successful') ? 'text-green-700' : 'text-red-700'}`}>
                  {error}
                </p>
              </div>
            </div>
          </div>
        )}
        
        <form className="mt-8 space-y-6" onSubmit={handleSubmit}>
          <div className="rounded-md shadow-xs -space-y-px">
            <div>
              <label htmlFor="username" className="sr-only">Username</label>
              <input
                id="username"
                type="text"
                className="appearance-none rounded-none relative block w-full px-3 py-2 border border-gray-300 placeholder-gray-500 text-gray-900 rounded-t-md focus:outline-hidden focus:ring-primary-500 focus:border-primary-500 focus:z-10 sm:text-sm"
                placeholder="Username"
                value={username}
                onChange={(e) => setUsername(e.target.value)}
                required
                autoComplete="username"
                disabled={loading}
              />
            </div>
            <div>
              <label htmlFor="password" className="sr-only">Password</label>
              <input
                id="password"
                type="password"
                className={`appearance-none rounded-none relative block w-full px-3 py-2 border border-gray-300 placeholder-gray-500 text-gray-900 ${!isLogin ? 'border-t-0' : 'rounded-b-md'} focus:outline-hidden focus:ring-primary-500 focus:border-primary-500 focus:z-10 sm:text-sm`}
                placeholder="Password"
                value={password}
                onChange={(e) => setPassword(e.target.value)}
                required
                autoComplete={isLogin ? "current-password" : "new-password"}
                disabled={loading}
              />
            </div>
            
            {!isLogin && (
              <div>
                <label htmlFor="adminPassword" className="sr-only">Admin Password</label>
                <input
                  id="adminPassword"
                  type="password"
                  className="appearance-none rounded-none relative block w-full px-3 py-2 border border-gray-300 placeholder-gray-500 text-gray-900 rounded-b-md border-t-0 focus:outline-hidden focus:ring-primary-500 focus:border-primary-500 focus:z-10 sm:text-sm"
                  placeholder="Admin Password"
                  value={adminPassword}
                  onChange={(e) => setAdminPassword(e.target.value)}
                  required
                  autoComplete="off"
                  disabled={loading}
                />
              </div>
            )}
          </div>
          
          <div>
            <button 
              type="submit" 
              className="group relative w-full flex justify-center py-2 px-4 border border-transparent text-sm font-medium rounded-md text-white bg-primary-600 hover:bg-primary-700 focus:outline-hidden focus:ring-2 focus:ring-offset-2 focus:ring-primary-500 disabled:opacity-50"
              disabled={loading}
            >
              {loading ? (
                <>
                  <span className="absolute left-0 inset-y-0 flex items-center pl-3">
                    <svg className="animate-spin -ml-1 mr-3 h-5 w-5 text-white" xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24">
                      <circle className="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="4"></circle>
                      <path className="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z"></path>
                    </svg>
                  </span>
                  Processing...
                </>
              ) : (
                isLogin ? 'Sign in' : 'Register'
              )}
            </button>
          </div>
        </form>
        
        <div className="text-center">
          <button 
            onClick={() => setIsLogin(!isLogin)}
            className="font-medium text-primary-600 hover:text-primary-500"
            disabled={loading}
          >
            {isLogin ? 'Need an account? Register' : 'Already have an account? Login'}
          </button>
        </div>
      </div>
    </div>
  );
}