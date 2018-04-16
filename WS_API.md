# Using WickedNotify in your application
----------------------------------------

WickedNotify exposes a Websocket Server on port 8989 by default. It is recommended to use a reverse proxy with SSL to secure the websocket.

Code samples in this document will be JavaScript.

## APNS devices
---------------

To start, connect to your WickedNotify instance
```js
var s = new WebSocket('/notifications');
```
When you receive a `CONNECTED` message, send your GUID like so:
```js
s.onmessage = function (message) {
    if (message.data === 'CONNECTED') {
        s.send('GUID=' + account_guid);
```
When you receive a `MATCH` message, send your APNS id like so:
```js
    } else if (message.data === 'MATCH') {
        s.send('APNS=' + apns_id);
```
If the GUID does not match, you will get a `NO MATCH` message
```js
    } else if (message.data === 'NO MATCH') {
        alert('Could not match account');
```
If different error occurs,  you will get an `ERROR` message, at this time, the `ERROR` message does not contain anything specific. But in general, this would be a problem with PostgreSQL or memory usage. In this case, it's a good idea to try a few more times.
```js
    } else if (message.data === 'ERROR') {
        alert('There was an error');
        s.close();
```
After WickedNotify registers your APNS id, it will send you an `OK` message. At this point, you should close the websocket.
```js
    } else if (message.data === 'OK') {
		s.close();
    }
};
```

## Other devices
----------------

Non-Apple devices must receive notifications through WebSockets.

To start, connect to your WickedNotify instance
```js
var s = new WebSocket('/notifications');
```
When you receive a `CONNECTED` message, send your GUID like so:
```js
s.onmessage = function (message) {
    if (message.data === 'CONNECTED') {
        s.send('GUID=' + account_guid);
```
When you receive a `MATCH` message, you may report your last received notification like so:
```js
    } else if (message.data === 'MATCH') {
        s.send('LATEST=' + newestNotificationID);
```
If the GUID does not match, you will get a `NO MATCH` message
```js
    } else if (message.data === 'NO MATCH') {
        alert('Could not match account');
```
If different error occurs,  you will get an `ERROR` message, at this time, the `ERROR` message does not contain anything specific. But in general, this would be a problem with PostgreSQL or memory usage. In this case, it's a good idea to try a few more times.
```js
    } else if (message.data === 'ERROR') {
        alert('There was an error');
		s.close();
```
At this point, all messages are notifications, if you sent the `LATEST` request, then any notifications in response to that request will be prefixed with `OLD `. Otherwise the messages are simple JSON in the form `{title: <title>, body: <body>, id: <notification-id>}`. The `LATEST` request is based on the `id` field in this message.
```js
    } else {
        var data = message.data;
        if (data.substring(0, 4) === 'OLD ') {
            data = data.substring(4);
        }
        data = JSON.parse(data);
        showNotification(data.title, data.body);
        newestNotificationID = data.id;
    }
};
```
