import { useState, useEffect } from 'react';
import { useRouter } from 'next/router';
import Link from 'next/link';
import Layout from '../components/Layout';
import KillSwitch from '../components/KillSwitch';
import { subscribeToAllParkingSpots, unsubscribeFromParkingSpots } from '../lib/database';
import withAuth from '../lib/withAuth';

function Home() {
  const router = useRouter();
  const [parkingSpots, setParkingSpots] = useState({});
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);

  useEffect(() => {
    // Initialize with loading state
    setLoading(true);
    setError(null);
    
    // Set up real-time subscription just to get summary data
    let spotsRef;
    try {
      spotsRef = subscribeToAllParkingSpots((result) => {
        if (result.success) {
          setParkingSpots(result.data || {});
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

  // We don't need user handling here, it's handled by withAuth

  return (
    <Layout title="Dashboard - Smart Parking System">
      {/* Status overview */}
      <div className="bg-white shadow-xs border-b border-gray-200 rounded-lg mb-6 p-4">
        <div className="flex flex-col md:flex-row justify-between items-start md:items-center gap-4">
          <div className="flex items-center">
            <div className="bg-green-100 rounded-full w-3 h-3 flex items-center justify-center mr-2">
              <div className={`w-2 h-2 rounded-full ${parkingSpots.Devicemaster?.isOnline ? 'bg-green-500 animate-pulse' : 'bg-red-500'}`}></div>
            </div>
            <span className="text-sm font-medium">
              Master ESP32: {parkingSpots.Devicemaster?.isOnline ? 'Online' : 'Offline - Not Reachable'}
            </span>
          </div>
          
          <div className="flex flex-wrap gap-3">
            {/* Calculate counts on the fly */}
            <div className="flex items-center gap-2 bg-green-100 px-3 py-1 rounded-full">
              <div className="w-2 h-2 rounded-full bg-green-500"></div>
              <span className="text-sm font-medium">
                {Object.values(parkingSpots).filter(spot => 
                  spot['System Status'] !== 'offline' && 
                  spot['Parking Status'] === 'open' && 
                  spot !== parkingSpots.Devicemaster &&
                  spot !== parkingSpots.DeviceCount
                ).length} Available
              </span>
            </div>
            <div className="flex items-center gap-2 bg-red-100 px-3 py-1 rounded-full">
              <div className="w-2 h-2 rounded-full bg-red-500"></div>
              <span className="text-sm font-medium">
                {Object.values(parkingSpots).filter(spot => 
                  spot['System Status'] !== 'offline' && 
                  spot['Parking Status'] === 'occupied' && 
                  spot !== parkingSpots.Devicemaster &&
                  spot !== parkingSpots.DeviceCount
                ).length} Occupied
              </span>
            </div>
            <div className="flex items-center gap-2 bg-gray-100 px-3 py-1 rounded-full">
              <div className="w-2 h-2 rounded-full bg-gray-500"></div>
              <span className="text-sm font-medium">
                {Object.values(parkingSpots).filter(spot => 
                  spot['System Status'] === 'offline' && 
                  spot !== parkingSpots.Devicemaster &&
                  spot !== parkingSpots.DeviceCount
                ).length} Offline
              </span>
            </div>
            {Object.values(parkingSpots).some(spot => spot['Sensor Error'] && spot !== parkingSpots.Devicemaster && spot !== parkingSpots.DeviceCount) && (
              <div className="flex items-center gap-2 bg-yellow-100 px-3 py-1 rounded-full">
                <div className="w-2 h-2 rounded-full bg-yellow-500"></div>
                <span className="text-sm font-medium">
                  {Object.values(parkingSpots).filter(spot => 
                    spot['Sensor Error'] && 
                    spot !== parkingSpots.Devicemaster &&
                    spot !== parkingSpots.DeviceCount
                  ).length} Error
                </span>
              </div>
            )}
          </div>
        </div>
      </div>

      {/* Kill Switch Component */}
      <div className="mb-6">
        <KillSwitch />
      </div>

      {/* Dashboard Cards */}
      <div className="grid grid-cols-1 md:grid-cols-3 gap-6">
        {/* Parking Status Card */}
        <Link href="/parking" legacyBehavior>
          <a className="card hover:shadow-lg transition-shadow">
            <div className="p-6">
              <div className="flex justify-between items-start">
                <div>
                  <h3 className="text-lg font-semibold text-gray-900">Parking Spots</h3>
                  <p className="text-sm text-gray-500 mb-4">View and manage all parking spots</p>
                </div>
                <div className="bg-primary-50 p-3 rounded-full">
                  <svg xmlns="http://www.w3.org/2000/svg" className="h-6 w-6 text-primary-600" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                    <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M5 8h14M5 8a2 2 0 110-4h14a2 2 0 110 4M5 8v10a2 2 0 002 2h10a2 2 0 002-2V8m-9 4h4" />
                  </svg>
                </div>
              </div>
              <div className="mt-4">
                <div className="flex justify-between mb-1">
                  <span className="text-xs font-medium text-gray-700">Occupancy</span>
                  <span className="text-xs font-medium text-gray-700">
                    {Object.values(parkingSpots).filter(spot => 
                      spot['System Status'] !== 'offline' && 
                      spot['Parking Status'] === 'occupied' && 
                      spot !== parkingSpots.Devicemaster
                    ).length} / {Object.keys(parkingSpots).filter(key => key.startsWith('Device') && key !== 'DeviceCount' && key !== 'Devicemaster' && (!parkingSpots[key].removed)).length || 1}
                  </span>
                </div>
                <div className="w-full bg-gray-200 rounded-full h-2">
                  {(() => {
                    const total = Object.keys(parkingSpots).filter(key => key.startsWith('Device') && key !== 'DeviceCount' && key !== 'Devicemaster' && (!parkingSpots[key].removed)).length || 1;
                    const occupied = Object.values(parkingSpots).filter(spot => 
                      spot['System Status'] !== 'offline' && 
                      spot['Parking Status'] === 'occupied' && 
                      spot !== parkingSpots.Devicemaster &&
                      spot !== parkingSpots.DeviceCount
                    ).length;
                    const percentage = (occupied / total) * 100;
                    return (
                      <div
                        className="bg-primary-600 h-2 rounded-full"
                        style={{ width: `${percentage}%` }}
                      ></div>
                    );
                  })()}
                </div>
              </div>
            </div>
          </a>
        </Link>

        {/* API Logs Card */}
        <Link href="/logs" legacyBehavior>
          <a className="card hover:shadow-lg transition-shadow">
            <div className="p-6">
              <div className="flex justify-between items-start">
                <div>
                  <h3 className="text-lg font-semibold text-gray-900">API Logs</h3>
                  <p className="text-sm text-gray-500 mb-4">View system API requests and responses</p>
                </div>
                <div className="bg-blue-50 p-3 rounded-full">
                  <svg xmlns="http://www.w3.org/2000/svg" className="h-6 w-6 text-blue-600" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                    <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M9 5H7a2 2 0 00-2 2v12a2 2 0 002 2h10a2 2 0 002-2V7a2 2 0 00-2-2h-2M9 5a2 2 0 002 2h2a2 2 0 002-2M9 5a2 2 0 012-2h2a2 2 0 012 2" />
                  </svg>
                </div>
              </div>
              <div className="mt-4 text-sm">
                <p className="flex items-center">
                  <span className="inline-block w-3 h-3 mr-2 bg-blue-500 rounded-full"></span>
                  View detailed API request logs
                </p>
                <p className="flex items-center mt-2">
                  <span className="inline-block w-3 h-3 mr-2 bg-blue-500 rounded-full"></span>
                  Track system performance and errors
                </p>
              </div>
            </div>
          </a>
        </Link>

        {/* Device Management Card */}
        <Link href="/devices" legacyBehavior>
          <a className="card hover:shadow-lg transition-shadow">
            <div className="p-6">
              <div className="flex justify-between items-start">
                <div>
                  <h3 className="text-lg font-semibold text-gray-900">Device Management</h3>
                  <p className="text-sm text-gray-500 mb-4">Manage sensor devices and settings</p>
                </div>
                <div className="bg-purple-50 p-3 rounded-full">
                  <svg xmlns="http://www.w3.org/2000/svg" className="h-6 w-6 text-purple-600" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                    <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M9 3v2m6-2v2M9 19v2m6-2v2M5 9H3m2 6H3m18-6h-2m2 6h-2M7 19h10a2 2 0 002-2V7a2 2 0 00-2-2H7a2 2 0 00-2 2v10a2 2 0 002 2zM9 9h6v6H9V9z" />
                  </svg>
                </div>
              </div>
              <div className="mt-4 text-sm">
                <p className="flex items-center">
                  <span className="inline-block w-3 h-3 mr-2 bg-purple-500 rounded-full"></span>
                  Configure device thresholds
                </p>
                <p className="flex items-center mt-2">
                  <span className="inline-block w-3 h-3 mr-2 bg-purple-500 rounded-full"></span>
                  Manage device mappings and registrations
                </p>
              </div>
            </div>
          </a>
        </Link>
      </div>

      {/* System Status */}
      <div className="card mt-6">
        <div className="p-6">
          <h3 className="text-lg font-semibold text-gray-900 mb-4">System Status</h3>
          {error ? (
            <div className="rounded-md bg-red-50 p-4">
              <div className="flex">
                <div className="shrink-0">
                  <svg className="h-5 w-5 text-red-400" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 20 20" fill="currentColor">
                    <path fillRule="evenodd" d="M10 18a8 8 0 100-16 8 8 0 000 16zM8.707 7.293a1 1 0 00-1.414 1.414L8.586 10l-1.293 1.293a1 1 0 101.414 1.414L10 11.414l1.293 1.293a1 1 0 001.414-1.414L11.414 10l1.293-1.293a1 1 0 00-1.414-1.414L10 8.586 8.707 7.293z" clipRule="evenodd" />
                  </svg>
                </div>
                <div className="ml-3">
                  <p className="text-sm text-red-700">Error loading system status: {error}</p>
                </div>
                <div className="ml-auto">
                  <button 
                    onClick={() => window.location.reload()}
                    className="btn-danger text-sm"
                  >
                    Retry
                  </button>
                </div>
              </div>
            </div>
          ) : loading ? (
            <div className="flex items-center justify-center py-8">
              <div className="animate-spin rounded-full h-8 w-8 border-b-2 border-primary-600"></div>
              <span className="ml-3 text-sm font-medium text-gray-700">Loading system status...</span>
            </div>
          ) : (
            <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
              <div className="bg-gray-50 rounded-lg p-4">
                <h4 className="text-sm font-medium text-gray-700 mb-2">Master Device Status</h4>
                <div className="flex items-center">
                  <div className={`w-3 h-3 rounded-full ${parkingSpots.Devicemaster?.isOnline ? 'bg-green-500' : 'bg-red-500'} mr-2`}></div>
                  <span className="text-sm">{parkingSpots.Devicemaster?.isOnline ? 'Online' : 'Offline'}</span>
                </div>
              </div>
              <div className="bg-gray-50 rounded-lg p-4">
                <h4 className="text-sm font-medium text-gray-700 mb-2">Connected Sensors</h4>
                <div className="text-2xl font-bold text-gray-900">
                  {Object.keys(parkingSpots).filter(key => key.startsWith('Device') && key !== 'DeviceCount' && key !== 'Devicemaster' && (!parkingSpots[key].removed)).length}
                </div>
              </div>
            </div>
          )}
        </div>
      </div>
    </Layout>
  );
}

export default withAuth(Home);