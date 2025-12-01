import { database } from './firebase';
import { ref, get, set, query, orderByChild, equalTo } from 'firebase/database';
import { logApiRequest } from './logHandler';
import { createHash, randomBytes } from 'crypto';

// Cookie management helpers
const COOKIE_NAME = 'smart_parking_session';
const COOKIE_MAX_AGE = 15 * 60; // 15 minutes in seconds

/**
 * Sets a cookie with the user session
 * @param {string} username - The username to store in the cookie
 */
const setSessionCookie = (username) => {
  if (typeof window === 'undefined') return;
  
  const expiresAt = new Date(Date.now() + COOKIE_MAX_AGE * 1000).toISOString();
  const sessionData = JSON.stringify({ username, expiresAt });
  
  // Set cookie with 15-minute expiration
  document.cookie = `${COOKIE_NAME}=${encodeURIComponent(sessionData)}; path=/; max-age=${COOKIE_MAX_AGE}; SameSite=Strict`;
};

/**
 * Gets the session data from cookie
 * @returns {object|null} - The session data or null if not found/expired
 */
const getSessionCookie = () => {
  if (typeof window === 'undefined') return null;
  
  const cookies = document.cookie.split(';');
  const sessionCookie = cookies.find(cookie => cookie.trim().startsWith(`${COOKIE_NAME}=`));
  
  if (!sessionCookie) return null;
  
  try {
    const cookieValue = sessionCookie.split('=')[1];
    const sessionData = JSON.parse(decodeURIComponent(cookieValue));
    
    // Check if session has expired
    if (new Date(sessionData.expiresAt) < new Date()) {
      clearSessionCookie();
      return null;
    }
    
    return sessionData;
  } catch (error) {
    console.error('Error parsing session cookie:', error);
    return null;
  }
};

/**
 * Clears the session cookie
 */
const clearSessionCookie = () => {
  if (typeof window === 'undefined') return;
  
  document.cookie = `${COOKIE_NAME}=; path=/; expires=Thu, 01 Jan 1970 00:00:00 GMT`;
};

/**
 * Refreshes the session by extending the cookie expiration
 * Call this on user activity to keep the session alive
 */
export const refreshSession = () => {
  const currentUser = getCurrentUser();
  if (currentUser) {
    setSessionCookie(currentUser.username);
  }
};

/**
 * Hashes a password with the provided salt using SHA-256
 * @param {string} password - The password to hash
 * @param {string} salt - The salt to use for hashing
 * @returns {string} - The hashed password
 */
const hashPassword = (password, salt) => {
  return createHash('sha256').update(password + salt).digest('hex');
};

/**
 * Registers a new user
 * @param {string} username - The username to register
 * @param {string} password - The password to use
 * @param {string} adminPassword - The admin password for authorization
 * @returns {Promise<boolean>} - Whether the registration was successful
 */
export const register = async (username, password, adminPassword) => {
  try {
    // First verify admin password
    const adminConfigRef = ref(database, 'adminConfig');
    const adminConfigSnapshot = await get(adminConfigRef);
    
    // Check if this is the first user registration (no admin password set yet)
    const isFirstUserRegistration = !adminConfigSnapshot.exists() || !adminConfigSnapshot.val().adminPasswordHash;
    
    if (!isFirstUserRegistration) {
      // Get admin config values
      const adminConfig = adminConfigSnapshot.val();
      const hashedAdminPassword = hashPassword(adminPassword, adminConfig.adminSalt);
      
      // Log debugging info (will be removed in production)
      console.log('Admin password verification:');
      console.log('- Admin salt exists:', !!adminConfig.adminSalt);
      console.log('- Admin password hash exists:', !!adminConfig.adminPasswordHash);
      
      if (hashedAdminPassword !== adminConfig.adminPasswordHash) {
        // Admin password incorrect
        await logApiRequest(
          'system',
          '/auth/register',
          'POST',
          { username },
          false,
          'Invalid admin password'
        );
        return false;
      }
    } else {
      console.log('First user registration - bypassing admin password check');
    }
    
    // Check if username already exists
    const userQuery = query(
      ref(database, 'users'),
      orderByChild('username'),
      equalTo(username)
    );
    
    const snapshot = await get(userQuery);
    
    if (snapshot.exists()) {
      // Username already taken
      await logApiRequest(
        'system',
        '/auth/register',
        'POST',
        { username },
        false,
        'Username already exists'
      );
      return false;
    }
    
    // Generate salt and hash password
    const salt = randomBytes(16).toString('hex');
    const hashedPassword = hashPassword(password, salt);
    
    // Create new user in database
    const userRef = ref(database, `users/${username}`);
    await set(userRef, {
      username,
      passwordHash: hashedPassword,
      salt,
      createdAt: Date.now(),
      isAdmin: isFirstUserRegistration // Make the first user an admin
    });
    
    // If this is the first user, also set up the admin password
    if (isFirstUserRegistration) {
      // Set the admin password to be the same as the user's password for the first user
      const adminSalt = randomBytes(16).toString('hex');
      const adminPasswordHash = hashPassword(password, adminSalt);
      
      // Create or update admin config
      await set(adminConfigRef, {
        adminPasswordHash,
        adminSalt,
        updatedAt: Date.now(),
        updatedBy: username
      });
      
      console.log('First user registered, admin password automatically set');
    }
    
    // Log successful registration
    await logApiRequest(
      'system',
      '/auth/register',
      'POST',
      { username },
      true
    );
    
    return true;
  } catch (error) {
    console.error('Error registering user:', error);
    
    // Log failed registration
    await logApiRequest(
      'system',
      '/auth/register',
      'POST',
      { username },
      false,
      error.message
    );
    
    return false;
  }
};

/**
 * Logs in a user
 * @param {string} username - The username to log in with
 * @param {string} password - The password to use
 * @returns {Promise<boolean>} - Whether the login was successful
 */
export const login = async (username, password) => {
  try {
    // Get user from database
    const userRef = ref(database, `users/${username}`);
    const snapshot = await get(userRef);
    
    if (!snapshot.exists()) {
      // User not found
      await logApiRequest(
        'system',
        '/auth/login',
        'POST',
        { username },
        false,
        'Invalid username or password'
      );
      return false;
    }
    
    const user = snapshot.val();
    
    // Hash the provided password with the stored salt
    const hashedPassword = hashPassword(password, user.salt);
    
    // Log authentication details for debugging (remove in production)
    console.log('Login attempt:');
    console.log('- User salt exists:', !!user.salt);
    console.log('- Stored password hash length:', user.passwordHash?.length);
    console.log('- Generated password hash length:', hashedPassword?.length);
    console.log('- Passwords match:', hashedPassword === user.passwordHash);
    
    // Compare with stored hash
    if (hashedPassword !== user.passwordHash) {
      // Password incorrect
      await logApiRequest(
        'system',
        '/auth/login',
        'POST',
        { username },
        false,
        'Invalid username or password'
      );
      return false;
    }
    
    // Set session cookie
    setSessionCookie(username);
    
    // Update last login time
    await set(ref(database, `users/${username}/lastLogin`), Date.now());
    
    // Log successful login
    await logApiRequest(
      'system',
      '/auth/login',
      'POST',
      { username },
      true
    );
    
    return true;
  } catch (error) {
    console.error('Error logging in:', error);
    
    // Log failed login
    await logApiRequest(
      'system',
      '/auth/login',
      'POST',
      { username },
      false,
      error.message
    );
    
    return false;
  }
};

/**
 * Logs out the current user
 */
export const logout = async () => {
  try {
    const currentUser = getCurrentUser();
    if (currentUser) {
      // Log logout
      await logApiRequest(
        'system',
        '/auth/logout',
        'POST',
        { username: currentUser.username },
        true
      );
    }
    
    // Clear session cookie
    clearSessionCookie();
    
    return true;
  } catch (error) {
    console.error('Error logging out:', error);
    return false;
  }
};

/**
 * Checks if a user is currently logged in
 * @returns {boolean} - Whether a user is logged in
 */
export const isLoggedIn = () => {
  const sessionData = getSessionCookie();
  return sessionData !== null;
};

/**
 * Gets the current user
 * @returns {object|null} - The current user or null if not logged in
 */
export const getCurrentUser = () => {
  const sessionData = getSessionCookie();
  if (!sessionData) return null;
  
  return {
    username: sessionData.username,
    expiresAt: sessionData.expiresAt
  };
};

/**
 * Sets or updates the admin password
 * @param {string} adminPassword - The admin password to set
 * @param {string} currentUserPassword - Password of the current user (for verification)
 * @returns {Promise<boolean>} - Whether the operation was successful
 */
export const setAdminPassword = async (adminPassword, currentUserPassword) => {
  try {
    // Verify current user is logged in
    const currentUser = getCurrentUser();
    if (!currentUser) {
      console.error('No user logged in');
      return false;
    }
    
    // Verify current user's password
    const userRef = ref(database, `users/${currentUser.username}`);
    const userSnapshot = await get(userRef);
    
    if (!userSnapshot.exists()) {
      console.error('User not found');
      return false;
    }
    
    const user = userSnapshot.val();
    const hashedPassword = hashPassword(currentUserPassword, user.salt);
    
    if (hashedPassword !== user.passwordHash) {
      console.error('Invalid user password');
      return false;
    }
    
    // Set admin password
    const adminSalt = randomBytes(16).toString('hex');
    const adminPasswordHash = hashPassword(adminPassword, adminSalt);
    
    console.log('Setting new admin password:');
    console.log('- New admin salt:', adminSalt);
    console.log('- New admin password hash length:', adminPasswordHash.length);
    
    const adminConfigRef = ref(database, 'adminConfig');
    await set(adminConfigRef, {
      adminPasswordHash,
      adminSalt,
      updatedAt: Date.now(),
      updatedBy: currentUser.username
    });
    
    await logApiRequest(
      'system',
      '/auth/setAdminPassword',
      'POST',
      { username: currentUser.username },
      true
    );
    
    return true;
  } catch (error) {
    console.error('Error setting admin password:', error);
    
    const currentUser = getCurrentUser();
    await logApiRequest(
      'system',
      '/auth/setAdminPassword',
      'POST',
      { username: currentUser ? currentUser.username : 'unknown' },
      false,
      error.message
    );
    
    return false;
  }
};