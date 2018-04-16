#pragma once

#include "db_framework.h"
#include "util_string.h"
#include "util_list_queue.h"
#include "notification.h"
#include "common_util_sql.h"
#include <ev.h>

extern Queue *que_registration;
extern ev_check *global_registration_watcher;
extern bool bol_query_in_process;

typedef struct {
	char *str_id;
	size_t int_id_len;
	struct sock_ev_client *client;
	enum {
		REG_ACTION_GUID = 0,
		REG_ACTION_APNS = 1,
		REG_ACTION_LATEST = 2
	} type;
} Registration_action;

// Called when a websocket client sends a GUID, APNS, or LATEST request
//
// GUID: runs a query to verify the GUID and get the account PK
//	- callback: registration_guid_query_cb
//
// APNS: runs a query to delete any devices with that ID
//	- callback: registration_apns_delete_query_cb
//
// LATEST: runs a query to get all unread notifications after the given ID
//	- callback: notification_unsent_query_cb
void registration_cb(EV_P, ev_check *w, int revents);

// Does nothing
bool registration_guid_query_cb(EV_P, void *cb_data, DB_result *res);

// Runs an insert into the device table
bool registration_apns_delete_query_cb(EV_P, void *cb_data, DB_result *res);

// Does nothing
bool registration_apns_insert_query_cb(EV_P, void *cb_data, DB_result *res);
