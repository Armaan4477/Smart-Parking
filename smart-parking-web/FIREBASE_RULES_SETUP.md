# Firebase Database Rules Setup

To configure the Firebase Realtime Database with proper indexing for API logs, follow these steps:

## 1. Install the Firebase CLI (if not already installed)

```bash
npm install -g firebase-tools
```

## 2. Login to Firebase

```bash
firebase login
```

## 3. Initialize Firebase in your project (if not already done)

```bash
firebase init
```

Select "Database" when prompted for which Firebase features you want to set up.

## 4. Deploy the Database Rules

The `database.rules.json` file has been created in the project root with the following content:

```json
{
  "rules": {
    ".read": "auth != null",
    ".write": "auth != null",
    "api_logs": {
      ".indexOn": ["timestamp"],
      "$logId": {
        ".read": true,
        ".write": "auth != null"
      }
    }
  }
}
```

Deploy these rules using:

```bash
firebase deploy --only database
```

## 5. Verify the Rules

1. Go to the Firebase Console (https://console.firebase.google.com)
2. Select your project
3. Navigate to "Realtime Database" 
4. Select the "Rules" tab
5. Confirm that your rules have been updated with the index on the timestamp field

## About the Rules

These rules:
1. Add an index on the "timestamp" field in the "api_logs" collection for efficient querying
2. Allow reading API logs without authentication (useful for public APIs)
3. Require authentication for all write operations
4. Limit the API logs to a maximum of 40 entries (configured in the logHandler.js)

The indexing allows faster querying when ordering logs by timestamp, which is important for retrieving the latest logs and for cleaning up old logs.