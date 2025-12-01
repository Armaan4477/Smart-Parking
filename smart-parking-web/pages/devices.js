import Layout from '../components/Layout';
import KillSwitch from '../components/KillSwitch';
import DeviceManagement from '../components/DeviceManagement';
import withAuth from '../lib/withAuth';

function DevicesPage() {
  return (
    <Layout title="Device Management - Smart Parking System">
      {/* Kill Switch Component */}
      <div className="mb-6">
        <KillSwitch />
      </div>
      
      <div className="card">
        <h2 className="text-xl font-semibold text-gray-900 mb-4">Device Management</h2>
        <DeviceManagement />
      </div>
    </Layout>
  );
}

export default withAuth(DevicesPage);