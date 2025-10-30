import Layout from '../components/Layout';
import ApiLogs from '../components/ApiLogs';
import withAuth from '../lib/withAuth';

function LogsPage() {
  return (
    <Layout title="API Logs - Smart Parking System">
      <div className="card">
        <h2 className="text-xl font-semibold text-gray-900 mb-4">API Request Logs</h2>
        <ApiLogs />
      </div>
    </Layout>
  );
}

export default withAuth(LogsPage);