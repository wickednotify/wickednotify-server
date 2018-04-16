# Using WickedNotify in your Android application
------------------------------------------------

This tutorial assumes that you already have a Login Activity and access to the GUID of the user.

Here is what we are going to need:
- A `ConnectivityActionReceiver` Receiver
- A `RestartBackgroundNotificationServiceReceiver` Receiver
- A `BackgroundNotificationService` Service
- An activity to start the Service

## The Manifest:
----------------

Inside your `<application>`:
```xml
<service
    android:name="com.your.app.BackgroundNotificationService"
    android:enabled="true"
    android:directBootAware="true"
    android:exported="false"/>

<receiver
    android:name="com.your.app.RestartBackgroundNotificationService"
    android:enabled="true"
    android:directBootAware="true"
    android:exported="true"
    android:label="RestartServiceWhenStopped">
    <intent-filter>
        <action android:name="com.your.app.ActivityRecognition.RestartBackgroundNotificationService"/>
        <action android:name="android.intent.action.LOCKED_BOOT_COMPLETED" />
    </intent-filter>
</receiver>
```
Inside your `<manifest>`:
```xml
<uses-permission android:name="android.permission.WAKE_LOCK" />
<uses-permission android:name="android.permission.INTERNET" />
<uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />
```

Also, add `android:enabled="true"` to your `<application>` element. This will ensure the OS knows that it can start our service.
The `android:directBootAware="true"` allows our service to run before the phone has been unlocked.

## The Code
-----------

The `RestartBackgroundNotificationService` Receiver is going to be started on boot, and all it does is run the `BackgroundNotificationService`:
```java
public class RestartBackgroundNotificationService extends BroadcastReceiver {
    @Override
    public void onReceive(Context context, Intent intent) {
        Log.i("RestartBGService", "onReceive");
        context.startService(new Intent(context, BackgroundNotificationService.class));;
    }
}
```

The `BackgroundNotificationService` needs to open a WebSocket (example code uses TooTallNate's java_websocket package) and make the requests outlined in WS_API.md:
```java
public class BackgroundNotificationService extends Service {
    NotificationManagerCompat mNotificationManager;
    private WebSocketClient mWebSocketClient;
    public boolean mStayClosed = false;
    public boolean mReconnecting = false;
    ConnectivityActionReceiver mConnectivityReceiver;
    WakeLock mWakeLock;
    PowerManager mPowerManager;
    SharedPreferences mSharedPreferences;

    public BackgroundNotificationService(Context applicationContext) { super(); }
    public BackgroundNotificationService() {}

    @Override
    public int onStartCommand(Intent workIntent, int flags, int startId) {
        super.onStartCommand(workIntent, flags, startId);

        mNotificationManager = NotificationManagerCompat.from(this);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
        	// Create the NotificationChannel, but only on API 26+ because
        	// the NotificationChannel class is new and not in the support library
        	CharSequence name = "YourApp";
        	String description = "YourApp Notification channel";
        	NotificationChannel channel = new NotificationChannel("YourApp", name, NotificationManager.IMPORTANCE_HIGH);
        	channel.setDescription(description);
        	// Register the channel with the system
            NotificationManager systemNotificationManager = (NotificationManager)getSystemService(Context.NOTIFICATION_SERVICE);
            systemNotificationManager.createNotificationChannel(channel);
        }

        Context storageContext;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            // All N devices have split storage areas, but we may need to
            // move the existing preferences to the new device protected
            // storage area, which is where the data lives from now on.
            final Context deviceContext = this.createDeviceProtectedStorageContext();
            if (!deviceContext.moveSharedPreferencesFrom(this, "YourApp")) {
                Log.w("YourApp", "Failed to migrate shared preferences.");
            }
            storageContext = deviceContext;
        } else {
            storageContext = this;
        }

        // Get the preferences
        mSharedPreferences = storageContext.getSharedPreferences("YourApp", MODE_MULTI_PROCESS);
        if (mSharedPreferences.getString("GUID", null) != null) {
            mPowerManager = (PowerManager)getSystemService(POWER_SERVICE);
            mWakeLock = mPowerManager.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "Notification Listener");
            mWakeLock.acquire();

            mConnectivityReceiver = new ConnectivityActionReceiver();
            IntentFilter receiverIntentFilter = new IntentFilter("android.net.conn.CONNECTIVITY_CHANGE");
            receiverIntentFilter.addAction("android.net.wifi.STATE_CHANGE");
            this.registerReceiver(mConnectivityReceiver, receiverIntentFilter);

            connectWebSocket();

            return START_STICKY;
        } else {
            return START_NOT_STICKY;
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        Log.i("EXIT", "ondestroy!");
        if (mSharedPreferences.getString("GUID", null) != null) {
            Intent broadcastIntent = new Intent("com.your.app.ActivityRecognition.RestartBackgroundNotificationService");
            sendBroadcast(broadcastIntent);
        }
        mStayClosed = true;
        mWebSocketClient.close();
        mWakeLock.release();
    }

    private void connectWebSocket() {
        Log.i("Websocket", "connectWebSocket()");
        if (mWebSocketClient != null) {
            mWebSocketClient.close();
        }

        final BackgroundNotificationService self = this;
        URI uri;
        try {
            uri = new URI("wss://www.sunnyserve.com/notifications");
        } catch (URISyntaxException e) {
            e.printStackTrace();
            return;
        }

        ArrayList knownExtensions = new ArrayList();
        ArrayList knownProtocols = new ArrayList();
        knownProtocols.add(new Protocol(""));

        // Apparently, when you instantiate a class, you get to override methods right then and there
        // This is how the java_websockets library does the callbacks
        mWebSocketClient = new WebSocketClient(uri, new Draft_6455(knownExtensions, knownProtocols)) {
            @Override
            public void onOpen(ServerHandshake serverHandshake) {
                Log.i("Websocket", "Opened");
            }

            @Override
            public void onMessage(String message) {
                Log.i("Websocket", "Message " + message);

                if (message.equals("CONNECTED")) {
                    mWebSocketClient.send("GUID=" + mSharedPreferences.getString("GUID", null));
                } else if (message.equals("MATCH")) {
                    if (mSharedPreferences.getString("latestMessage", null) != null) {
                        mWebSocketClient.send("LATEST=" + mSharedPreferences.getString("latestMessage", null));
                    }

                } else if (message.contains("NO MATCH")) {
                    SharedPreferences.Editor editor = mSharedPreferences.edit();
                    editor.remove("GUID");
                    editor.commit();
                    stopSelf();
                    
                } else if (message.contains("ERROR")) {
                    this.close();
                } else if (message.contains("READ ")) {
                    mNotificationManager.cancel(Integer.parseInt(message.substring(5), 10));

                } else {
                    try {
                        boolean bolSound = true;
                        if (message.contains("OLD ")) {
                            message = message.substring(4);
                            bolSound = false;
                        }
                        JSONObject jsnNotification = new JSONObject(message);

                        SharedPreferences.Editor editor = mSharedPreferences.edit();
                        editor.putString("latestMessage", jsnNotification.getString("id"));
                        editor.commit();

                        NotificationCompat.Builder mBuilder = new NotificationCompat.Builder(self, "YourApp")
                                .setSmallIcon(R.drawable.notification_icon)
                                .setCategory(NotificationCompat.CATEGORY_MESSAGE)
                                .setContentTitle(jsnNotification.getString("title"))
                                .setContentText(jsnNotification.getString("body"))
                                .setPriority(NotificationCompat.PRIORITY_HIGH)
                                .setVisibility(NotificationCompat.VISIBILITY_PUBLIC)
                                .setStyle(new NotificationCompat.BigTextStyle().bigText(jsnNotification.getString("body")))
                                .setAutoCancel(true);

                        if (bolSound) {
                            mBuilder.setDefaults(NotificationCompat.DEFAULT_SOUND | NotificationCompat.DEFAULT_VIBRATE);
                        }

                        Intent intent = new Intent(self, MainActivity.class);
                        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
                        PendingIntent pi = PendingIntent.getActivity(self, 0, intent, 0);
                        mBuilder.setContentIntent(pi);

                        mNotificationManager.notify(Integer.parseInt(jsnNotification.getString("id"), 10), mBuilder.build());

                    } catch (JSONException e) {
                        e.printStackTrace();
                    }
                }
            }

            @Override
            public void onClose(int i, String s, boolean b) {
                Log.i("Websocket", "mStayClosed: " + mStayClosed + ", mReconnecting: " + mReconnecting);
                if (!mStayClosed && !mReconnecting) {
                    connectWebSocket();
                } else if (mReconnecting) {
                    mReconnecting = false;
                }
            }

            @Override
            public void onError(Exception e) {
                Log.i("Websocket", "Error " + e.getMessage());
                this.close();
            }
        };

        mWebSocketClient.setConnectionLostTimeout(30);

        mWebSocketClient.connect();
    }

    void reconnect() {
        //mStayClosed = false;
        Log.i("Websocket", "reconnect");
        mReconnecting = true;
        connectWebSocket();
    }

    private final IBinder mBinder = new BackgroundNotificationServiceBinder();
    public class BackgroundNotificationServiceBinder extends Binder {
        BackgroundNotificationService getService() {
            // Return this instance of LocalService so clients can call public methods
            return BackgroundNotificationService.this;
        }
    }

    @Override
    public IBinder onBind(Intent a) {
        return mBinder;
    }
}
```

The `ConnectivityActionReceiver` is started inside the `BackgroundNotificationService` and is run when you switch between Cell and Wi-Fi
```java
public class ConnectivityActionReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(Context context, Intent intent) {
        Log.i("ConnectivityReceiver", "onReceive");
        BackgroundNotificationService service = (BackgroundNotificationService)context;
        NetworkInfo networkInfo =
                intent.getParcelableExtra(ConnectivityManager.EXTRA_NETWORK_INFO);
        if(networkInfo.isConnected()) {
            service.reconnect();
        }
    }
}
```

Now, in your Activity, you need to tell the Service to start, and with what GUID to listen for notifications with.

In your Activity's onCreate, get the SharedPreferences:
```java
Context storageContext;
if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
    // All N devices have split storage areas, but we may need to
    // move the existing preferences to the new device protected
    // storage area, which is where the data lives from now on.
    final Context deviceContext = this.createDeviceProtectedStorageContext();
    if (!deviceContext.moveSharedPreferencesFrom(this, "YourApp")) {
        Log.w("YourApp", "Failed to migrate shared preferences.");
    }
    storageContext = deviceContext;
} else {
    storageContext = this;
}

mSharedPreferences = storageContext.getSharedPreferences("YourApp", MODE_MULTI_PROCESS);
```

And now, when you get your GUID, set it in the SharedPreferences like so:
```java
SharedPreferences.Editor editor = mSharedPreferences.edit();
editor.putString("GUID", strGUID);
editor.commit();
```

Then start your service!

## Closing Remarks

This should be all you need to get notifications in your Android app with WickedNotify!

If you have any questions, or something is missing, please post an Issue and I'll get back to you as soon as I can.
