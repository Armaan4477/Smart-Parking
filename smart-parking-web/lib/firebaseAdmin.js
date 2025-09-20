import admin from 'firebase-admin';
import { cert } from 'firebase-admin/app';
import { getDatabase } from 'firebase-admin/database';

// Initialize Firebase Admin
if (!admin.apps.length) {
  try {
    admin.initializeApp({
      credential: cert(require('./serviceAccountKey.json')),
      databaseURL: "https://your-project-id-default-rtdb.firebaseio.com"
    });
  } catch (error) {
    console.log('Firebase admin initialization error', error.stack);
  }
}

export default admin.database();