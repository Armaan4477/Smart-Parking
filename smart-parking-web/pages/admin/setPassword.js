import { useState } from 'react';
import { useRouter } from 'next/router';
import styles from '../../styles/Login.module.css';

export default function SetAdminPassword() {
  const [adminPassword, setAdminPassword] = useState('');
  const [currentUserPassword, setCurrentUserPassword] = useState('');
  const [error, setError] = useState(null);
  const [success, setSuccess] = useState(null);
  const [loading, setLoading] = useState(false);
  const router = useRouter();

  const handleSubmit = async (e) => {
    e.preventDefault();
    setError(null);
    setSuccess(null);
    setLoading(true);
    
    try {
      const response = await fetch('/api/auth/setAdminPassword', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          adminPassword,
          currentUserPassword
        }),
      });
      
      const data = await response.json();
      
      if (data.success) {
        setSuccess('Admin password set successfully');
        setAdminPassword('');
        setCurrentUserPassword('');
      } else {
        setError(data.error || 'Failed to set admin password');
      }
    } catch (err) {
      console.error('Error setting admin password:', err);
      setError(err.message || 'Failed to set admin password');
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className={styles.loginContainer}>
      <div className={styles.loginForm}>
        <div className={styles.logoContainer}>
          <img src="/ouricon.png" alt="Smart Parking Logo" className={styles.logo} />
          <h1>Smart Parking System</h1>
        </div>

        <h2>Set Admin Password</h2>
        
        {error && <div className={styles.error}>{error}</div>}
        {success && <div className={styles.success}>{success}</div>}
        
        <p className={styles.description}>
          Set the admin password that will be used for creating new user accounts.
        </p>

        <form onSubmit={handleSubmit}>
          <div className={styles.inputGroup}>
            <label htmlFor="adminPassword">New Admin Password</label>
            <input
              id="adminPassword"
              type="password"
              value={adminPassword}
              onChange={(e) => setAdminPassword(e.target.value)}
              required
              autoComplete="new-password"
              disabled={loading}
            />
          </div>
          
          <div className={styles.inputGroup}>
            <label htmlFor="currentUserPassword">Your Password (for verification)</label>
            <input
              id="currentUserPassword"
              type="password"
              value={currentUserPassword}
              onChange={(e) => setCurrentUserPassword(e.target.value)}
              required
              autoComplete="current-password"
              disabled={loading}
            />
            <small className={styles.helpText}>Enter your current user password to confirm this change</small>
          </div>
          
          <button 
            type="submit" 
            className={styles.loginButton}
            disabled={loading}
          >
            {loading ? 'Processing...' : 'Set Admin Password'}
          </button>
        </form>
        
        <div className={styles.switchMode}>
          <button 
            onClick={() => router.push('/')}
            className={styles.switchButton}
            disabled={loading}
          >
            Return to Dashboard
          </button>
        </div>
      </div>
    </div>
  );
}