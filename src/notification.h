#pragma once

#include "apns.h"
#include "db_framework.h"
#include "util_string.h"
#include "util_list_queue.h"
#include "common_server.h"
#include "common_util_sql.h"
#include <ev.h>

extern ev_io *global_notify_watcher;
extern ev_prepare *global_reconnect_timer;
extern ev_tstamp global_close_time;
extern DB_conn *global_conn;

// Runs the LISTEN query
bool global_listen_cb(EV_P, void *cb_data, DB_result *res);

// This watcher runs PQconsumeInput ti recieve NOTIFYs
void global_notify_cb(EV_P, ev_io *w, int revents);

// This function parses the NOTIFYs
// NEW: runs a query to get the notification, along with all APNS devices
//  - callback: notification_query_cb
// READ: runs a query to get the profile id
//  - callback: notification_read_query_cb
void send_notices(EV_P);

// This function sends the notification to all clients and all APNS devices
bool notification_query_cb(EV_P, void *cb_data, DB_result *res);

// This function will cut a reconnect attempt off at 32 seconds
void client_reconnect_timer_cb(EV_P, ev_prepare *w, int revents);

// This function attempts to reconnect to PG
void global_conn_reset_cb(EV_P, ev_io *w, int revents);

// This function is the callback to all APNS requests,
// it checks to see if APNS said that the token was invalid.
// If so, it deletes it from the device table
//  - callback: notification_apns_delete_query_cb
void notification_cb(EV_P, APNS_notification_response *response, void *cb_data);

// Does nothing
bool notification_apns_delete_query_cb(EV_P, void *cb_data, DB_result *res);

// This functions sends unsent notifications to the device requesting them
bool notification_unsent_query_cb(EV_P, void *cb_data, DB_result *res);

// This function gets called when we get a READ NOTIFY, it forwards the READ message to all WS clients
bool notification_read_query_cb(EV_P, void *cb_data, DB_result *res);

// This is called when the PG connection succeeds
void global_connect_cb(EV_P, void *cb_data, DB_conn *conn);
