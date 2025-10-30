import { useState, useEffect } from 'react';
import Layout from '../components/Layout';
import { subscribeToAllParkingSpots, unsubscribeFromParkingSpots } from '../lib/database';
import withAuth from '../lib/withAuth';

function ParkingPage() {
  const [parkingSpots, setParkingSpots] = useState({});
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [updatedSpots, setUpdatedSpots] = useState({});

  useEffect(() => {
    // Initialize with loading state
    setLoading(true);
    setError(null);
    
    // Set up real-time subscription
    let spotsRef;
    try {
      spotsRef = subscribeToAllParkingSpots((result) => {
        if (result.success) {
          // Track which spots have been updated for visual feedback
          const newSpots = result.data || {};
          
          setParkingSpots(prevSpots => {
            // Find spots that have changed status
            const changedSpots = {};
            Object.entries(newSpots).forEach(([key, spot]) => {
              if (prevSpots[key] && prevSpots[key]['Parking Status'] !== spot['Parking Status']) {
                changedSpots[key] = true;
              }
            });
            
            // Remember updated spots for visual feedback
            if (Object.keys(changedSpots).length > 0) {
              setUpdatedSpots(changedSpots);
              // Clear the highlight after 2 seconds
              setTimeout(() => {
                setUpdatedSpots({});
              }, 2000);
            }
            
            return newSpots;
          });
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

  // Get counts of parking spots by status
  const spotCounts = Object.entries(parkingSpots)
    .filter(([key]) => key.startsWith('Device') && key !== 'DeviceCount' && key !== 'Devicemaster' && !parkingSpots[key].removed)
    .reduce(
      (acc, [_, spot]) => {
        if (spot['System Status'] === 'offline') {
          acc.offline++;
        } else if (spot['Parking Status'] === 'occupied') {
          acc.occupied++;
        } else if (spot['Parking Status'] === 'open') {
          acc.available++;
        }
        
        if (spot['Sensor Error']) {
          acc.error++;
        }
        
        return acc;
      },
      { available: 0, occupied: 0, offline: 0, error: 0 }
    );

  return (
    <Layout title="Parking Spots - Smart Parking System">
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
            <div className="flex items-center gap-2 bg-green-100 px-3 py-1 rounded-full">
              <div className="w-2 h-2 rounded-full bg-green-500"></div>
              <span className="text-sm font-medium">{spotCounts.available} Available</span>
            </div>
            <div className="flex items-center gap-2 bg-red-100 px-3 py-1 rounded-full">
              <div className="w-2 h-2 rounded-full bg-red-500"></div>
              <span className="text-sm font-medium">{spotCounts.occupied} Occupied</span>
            </div>
            <div className="flex items-center gap-2 bg-gray-100 px-3 py-1 rounded-full">
              <div className="w-2 h-2 rounded-full bg-gray-500"></div>
              <span className="text-sm font-medium">{spotCounts.offline} Offline</span>
            </div>
            {spotCounts.error > 0 && (
              <div className="flex items-center gap-2 bg-yellow-100 px-3 py-1 rounded-full">
                <div className="w-2 h-2 rounded-full bg-yellow-500"></div>
                <span className="text-sm font-medium">{spotCounts.error} Error</span>
              </div>
            )}
          </div>
        </div>
      </div>

      {/* Parking Spots */}
      <div className="card">
        <h2 className="text-xl font-semibold text-gray-900 mb-4">Parking Spots Status</h2>
        
        {error ? (
          <div className="rounded-md bg-red-50 p-4 mb-4">
            <div className="flex">
              <div className="shrink-0">
                <svg className="h-5 w-5 text-red-400" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 20 20" fill="currentColor">
                  <path fillRule="evenodd" d="M10 18a8 8 0 100-16 8 8 0 000 16zM8.707 7.293a1 1 0 00-1.414 1.414L8.586 10l-1.293 1.293a1 1 0 101.414 1.414L10 11.414l1.293 1.293a1 1 0 001.414-1.414L11.414 10l1.293-1.293a1 1 0 00-1.414-1.414L10 8.586 8.707 7.293z" clipRule="evenodd" />
                </svg>
              </div>
              <div className="ml-3">
                <p className="text-sm text-red-700">Error loading parking spots: {error}</p>
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
            <span className="ml-3 text-sm font-medium text-gray-700">Loading parking data...</span>
          </div>
        ) : Object.keys(parkingSpots).filter(key => key.startsWith('Device') && key !== 'DeviceCount' && key !== 'Devicemaster' && (!parkingSpots[key].removed)).length > 0 ? (
          <div className="grid grid-cols-1 sm:grid-cols-2 md:grid-cols-3 lg:grid-cols-4 gap-4">
            {Object.entries(parkingSpots)
              .filter(([key]) => key.startsWith('Device') && key !== 'DeviceCount' && key !== 'Devicemaster' && (!parkingSpots[key].removed))
              .map(([key, spot]) => {
              const deviceId = key.replace('Device', '');
              const isUpdated = updatedSpots[key];
              
              let statusClasses = 'bg-green-50 border-green-200';
              let statusIconClasses = 'bg-green-500';
              let statusText = 'Available';
              
              if (spot['System Status'] === 'offline') {
                statusClasses = 'bg-gray-50 border-gray-200';
                statusIconClasses = 'bg-gray-500';
                statusText = 'Offline';
              } else if (spot['Parking Status'] === 'occupied') {
                statusClasses = 'bg-red-50 border-red-200';
                statusIconClasses = 'bg-red-500';
                statusText = 'Occupied';
              } else if (spot['Parking Status'] === 'open') {
                // Keep the default green styling for 'open' spots
                statusClasses = 'bg-green-50 border-green-200';
                statusIconClasses = 'bg-green-500';
                statusText = 'Available';
              }
              
              if (spot['Sensor Error']) {
                statusClasses = 'bg-yellow-50 border-yellow-200';
                statusIconClasses = 'bg-yellow-500';
                statusText = 'Error';
              }
              
              if (isUpdated) {
                statusClasses += ' ring-2 ring-primary-500 ring-offset-2';
              }
              
              return (
                <div 
                  key={deviceId} 
                  className={`border rounded-lg overflow-hidden transition-all duration-300 ${statusClasses}`}
                >
                  <div className="p-4">
                    <div className="flex justify-between items-center mb-3">
                      <h3 className="text-lg font-bold">Spot {deviceId}</h3>
                      <div className={`w-3 h-3 rounded-full ${statusIconClasses}`}></div>
                    </div>
                    
                    <div className="space-y-2">
                      <p className="text-sm">
                        <span className="font-medium">Status:</span> {spot['Parking Status'] === 'open' ? 'Available' : 
                           spot['Parking Status'] === 'occupied' ? 'Occupied' : spot['Parking Status']}
                      </p>
                      <p className="text-sm">
                        <span className="font-medium">System:</span> {spot['System Status'] || 'unknown'}
                      </p>
                      
                      {spot['Sensor Error'] && (
                        <p className="text-sm text-yellow-700 bg-yellow-100 px-2 py-1 rounded-md inline-block">
                          Sensor Error
                        </p>
                      )}
                      
                      {spot['timeSinceUpdateMs'] && spot['System Status'] === 'offline' && (
                        <p className="text-xs text-gray-500">
                          Last seen: {Math.floor(spot['timeSinceUpdateMs'] / 1000)} seconds ago
                        </p>
                      )}
                    </div>
                  </div>
                  
                  {isUpdated && (
                    <div className="w-full bg-primary-500 text-white text-xs font-medium text-center py-1">
                      Updated
                    </div>
                  )}
                </div>
              );
            })}
          </div>
        ) : (
          <div className="text-center py-8 text-gray-500">
            <svg xmlns="http://www.w3.org/2000/svg" className="h-12 w-12 mx-auto text-gray-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1} d="M8 16l2.879-2.879m0 0a3 3 0 104.243-4.242 3 3 0 00-4.243 4.242zM21 12a9 9 0 11-18 0 9 9 0 0118 0z" />
            </svg>
            <p className="mt-2">No parking spots registered yet.</p>
          </div>
        )}
      </div>
    </Layout>
  );
}

export default withAuth(ParkingPage);