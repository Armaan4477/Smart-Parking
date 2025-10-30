import { useState, useEffect } from 'react';
import Link from 'next/link';
import { useRouter } from 'next/router';
import { logout, getCurrentUser } from '../lib/auth';

export default function Navigation({ isSidebarOpen, setIsSidebarOpen }) {
  const router = useRouter();
  const [user, setUser] = useState(null);

  // Get the current user on component mount
  useEffect(() => {
    setUser(getCurrentUser());
  }, []);

  // Handle logout
  const handleLogout = async () => {
    const success = await logout();
    if (success) {
      router.push('/login');
    }
  };

  return (
    <>
      {/* Header */}
      <header className="bg-white shadow-md z-10 relative">
        <div className="max-w-7xl mx-auto pl-0 pr-4 sm:pr-6 lg:pr-8 py-4 flex items-center justify-between">
          <div className="flex items-center">
            <button
              onClick={() => setIsSidebarOpen(!isSidebarOpen)}
              className={`text-gray-600 hover:text-gray-900 focus:outline-hidden focus:ring-2 focus:ring-primary-500 rounded-md p-1 transition-all ${
                !isSidebarOpen ? 'bg-gray-100 hover:bg-gray-200' : ''
              }`}
              aria-label="Toggle sidebar"
            >
              <svg xmlns="http://www.w3.org/2000/svg" className="h-6 w-6" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                {isSidebarOpen ? (
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M6 18L18 6M6 6l12 12" />
                ) : (
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M4 6h16M4 12h16M4 18h16" />
                )}
              </svg>
            </button>
            <div className="flex items-center ml-2">
              <img src="/ouricon.png" alt="Smart Parking Icon" className="h-8 w-8 mr-2" />
              <h1 className="text-xl font-bold text-primary-600">Smart Parking System</h1>
            </div>
          </div>
          
          {user && (
            <div className="flex items-center text-sm font-medium">
              <span className="hidden md:inline mr-4">Logged in as: {user.username}</span>
              <button onClick={handleLogout} className="btn-primary">
                Logout
              </button>
            </div>
          )}
        </div>
      </header>

      {/* Overlay for mobile when sidebar is open */}
      {isSidebarOpen && (
        <div 
          className="md:hidden fixed inset-0 bg-gray-600 bg-opacity-50 z-10 transition-opacity duration-300"
          onClick={() => setIsSidebarOpen(false)}
        ></div>
      )}

      {/* Sidebar - Fixed position and full height */}
      <div 
        className={`fixed left-0 top-0 h-full bg-white shadow-lg border-r border-gray-200 z-20 pt-16 transition-all duration-300 ease-in-out ${
          isSidebarOpen ? 'w-64 translate-x-0' : 'w-0 -translate-x-full opacity-0 invisible pointer-events-none'
        }`}
      >
        <div className="p-6 overflow-y-auto h-full">
          <h2 className="text-lg font-medium text-gray-900 mb-6">Menu</h2>
          <nav className="space-y-3">
            <Link href="/" legacyBehavior>
              <a className={`flex items-center px-4 py-3 text-sm font-medium rounded-lg ${
                router.pathname === "/" ? 'bg-primary-50 text-primary-700 shadow-sm' : 'text-gray-700 hover:bg-gray-50'
              }`}>
                <svg xmlns="http://www.w3.org/2000/svg" className="h-5 w-5 mr-3" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M3 12l2-2m0 0l7-7 7 7M5 10v10a1 1 0 001 1h3m10-11l2 2m-2-2v10a1 1 0 01-1 1h-3m-6 0a1 1 0 001-1v-4a1 1 0 011-1h2a1 1 0 011 1v4a1 1 0 001 1m-6 0h6" />
                </svg>
                Dashboard
              </a>
            </Link>
              <Link href="/parking" legacyBehavior>
                <a className={`flex items-center px-4 py-3 text-sm font-medium rounded-lg ${
                  router.pathname === "/parking" ? 'bg-primary-50 text-primary-700 shadow-sm' : 'text-gray-700 hover:bg-gray-50'
                }`}>
                  <svg xmlns="http://www.w3.org/2000/svg" className="h-5 w-5 mr-3" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                    <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M5 8h14M5 8a2 2 0 110-4h14a2 2 0 110 4M5 8v10a2 2 0 002 2h10a2 2 0 002-2V8m-9 4h4" />
                  </svg>
                  Parking Spots
                </a>
              </Link>
              <Link href="/logs" legacyBehavior>
                <a className={`flex items-center px-4 py-3 text-sm font-medium rounded-lg ${
                  router.pathname === "/logs" ? 'bg-primary-50 text-primary-700 shadow-sm' : 'text-gray-700 hover:bg-gray-50'
                }`}>
                  <svg xmlns="http://www.w3.org/2000/svg" className="h-5 w-5 mr-3" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                    <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M9 5H7a2 2 0 00-2 2v12a2 2 0 002 2h10a2 2 0 002-2V7a2 2 0 00-2-2h-2M9 5a2 2 0 002 2h2a2 2 0 002-2M9 5a2 2 0 012-2h2a2 2 0 012 2" />
                  </svg>
                  API Logs
                </a>
              </Link>
              <Link href="/devices" legacyBehavior>
                <a className={`flex items-center px-4 py-3 text-sm font-medium rounded-lg ${
                  router.pathname === "/devices" ? 'bg-primary-50 text-primary-700 shadow-sm' : 'text-gray-700 hover:bg-gray-50'
                }`}>
                  <svg xmlns="http://www.w3.org/2000/svg" className="h-5 w-5 mr-3" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                    <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M9 3v2m6-2v2M9 19v2m6-2v2M5 9H3m2 6H3m18-6h-2m2 6h-2M7 19h10a2 2 0 002-2V7a2 2 0 00-2-2H7a2 2 0 00-2 2v10a2 2 0 002 2zM9 9h6v6H9V9z" />
                  </svg>
                  Device Management
                </a>
              </Link>
            </nav>
          </div>
        </div>
    </>
  );
}