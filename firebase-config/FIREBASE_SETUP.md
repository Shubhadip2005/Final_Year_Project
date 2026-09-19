# Firebase Setup Guide for Antenna System

## Step 1: Create Firebase Project

1. Go to **https://console.firebase.google.com**
2. Click **"Add Project"**
3. Enter project name (e.g., "antenna-measurement-system")
4. Click **Create Project**

## Step 2: Set Up Realtime Database

1. In Firebase Console, click **Realtime Database** (left sidebar)
2. Click **Create Database**
3. Choose location (close to you)
4. Start in **Test Mode** (for development)
5. Click **Enable**

## Step 3: Set Database Rules

Replace default rules with:

```json
{
  "rules": {
    "measurements": {
      ".read": true,
      ".write": true,
      "$uid": {
        ".validate": "newData.hasChildren(['angle', 'current_reading', 'timestamp'])"
      }
    }
  }
}
```

## Step 4: Get Firebase Config

1. Go to **Project Settings** (⚙️ icon, top-right)
2. Click **Your apps** section
3. Click **"<></>"** to add Web app
4. Register app (give it a name)
5. Copy the config object

Example config:
```javascript
const firebaseConfig = {
  apiKey: "AIzaSyD...",
  authDomain: "antenna-project.firebaseapp.com",
  projectId: "antenna-project",
  storageBucket: "antenna-project.appspot.com",
  messagingSenderId: "123456789",
  appId: "1:123456789:web:abc123def456"
};
```

## Step 5: Create `.env.local` file

In your React app directory, create `.env.local`:

```
REACT_APP_FIREBASE_API_KEY=YOUR_API_KEY
REACT_APP_FIREBASE_AUTH_DOMAIN=your-project.firebaseapp.com
REACT_APP_FIREBASE_PROJECT_ID=your-project-id
REACT_APP_FIREBASE_STORAGE_BUCKET=your-project.appspot.com
REACT_APP_FIREBASE_MESSAGING_SENDER_ID=123456789
REACT_APP_FIREBASE_APP_ID=1:123456789:web:abc123
REACT_APP_FIREBASE_DATABASE_URL=https://your-project.firebaseio.com
```

## Database Structure

Your Firebase database will look like:
```
measurements/
├── 1695123456789/
│   ├── angle: 0
│   ├── current_reading: 14.7871
│   └── timestamp: "2024-01-15T10:30:45.123Z"
├── 1695123486789/
│   ├── angle: 10
│   ├── current_reading: 15.2345
│   └── timestamp: "2024-01-15T10:31:15.456Z"
...
```

## Testing Firebase Connection

1. Open browser DevTools (F12)
2. Check Console for Firebase messages
3. Visit `http://localhost:3000`
4. Should show "Connected" status

## Troubleshooting

- **Error: "No rules for user"**: Update database rules (Step 3)
- **Connection timeout**: Check if Firebase URL is correct
- **CORS error**: Firebase should handle CORS automatically