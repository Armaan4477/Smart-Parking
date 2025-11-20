import { initializeApp } from 'firebase/app';
import { getDatabase } from 'firebase/database';
import firebaseConfig from './firebaseConfig';

// Initialize Firebase
const firebaseApp = initializeApp(firebaseConfig);
const database = getDatabase(firebaseApp);

// Re-export kill switch for convenience
export { KILL_SWITCH_ENABLED } from '../context/KillSwitchContext';
export { database };