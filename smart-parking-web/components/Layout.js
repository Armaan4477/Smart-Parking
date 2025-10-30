import { useState, useEffect } from 'react';
import Head from 'next/head';
import Navigation from './Navigation';

export default function Layout({ children, title = 'Smart Parking System' }) {
  const [isSidebarOpen, setIsSidebarOpen] = useState(true);
  const [isMobile, setIsMobile] = useState(false);

  // Check if we're on mobile and close sidebar by default
  useEffect(() => {
    const checkMobile = () => {
      setIsMobile(window.innerWidth < 768);
    };
    
    // Initial check
    checkMobile();
    
    // Close sidebar by default on mobile
    if (window.innerWidth < 768) {
      setIsSidebarOpen(false);
    }

    // Add resize listener
    window.addEventListener('resize', checkMobile);
    
    // Clean up
    return () => window.removeEventListener('resize', checkMobile);
  }, []);

  return (
    <div className="min-h-screen bg-gray-100 flex flex-col">
      <Head>
        <title>{title}</title>
        <meta name="description" content="Smart Parking System API and Dashboard" />
        <link rel="icon" href="/favicon.ico" sizes="any" />
        <link rel="apple-touch-icon" href="/apple-touch-icon.png" />
        <link rel="manifest" href="/site.webmanifest" />
      </Head>

      <Navigation isSidebarOpen={isSidebarOpen} setIsSidebarOpen={setIsSidebarOpen} />

      {/* Main content - with transition */}
      <div className={`flex-1 flex pt-16 transition-all duration-300 ease-in-out ${isSidebarOpen ? 'md:ml-64' : ''}`}>
        {/* Content area */}
        <main className="flex-1 overflow-y-auto p-4 md:p-6 w-full">
          {children}
        </main>
      </div>

      {/* Footer - with transition */}
      <footer className={`bg-white border-t border-gray-200 transition-all duration-300 ease-in-out ${isSidebarOpen ? 'md:ml-64' : ''}`}>
        <div className="max-w-7xl mx-auto py-4 px-4 sm:px-6 lg:px-8 text-center text-gray-600 text-sm">
          Smart Parking System - ESP32 Firebase Integration
        </div>
      </footer>
    </div>
  );
}