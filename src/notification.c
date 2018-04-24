#include "notification.h"

ev_io *global_notify_watcher = NULL;
ev_prepare *global_reconnect_timer = NULL;
ev_tstamp global_close_time;
DB_conn *global_conn = NULL;

// Handle global PG connection
void global_connect_cb(EV_P, void *cb_data, DB_conn *conn) {
	(void)cb_data;
	SERROR_CHECK(conn->int_status == 1, "Could not connect to PostgreSQL: %s", conn->str_response);

	bol_query_in_process = true;
	SERROR_CHECK(DB_exec(EV_A, global_conn, NULL, "LISTEN ddt_notify;", global_listen_cb), "DB_exec failed");	

	return;
error:
	SFREE(conn->str_response);
	program_exit();
	return;
}

bool global_listen_cb(EV_P, void *cb_data, DB_result *res) {
	(void)cb_data;
	char *_str_response = NULL;

	SERROR_CHECK(res != NULL, "DB_exec failed");
	SERROR_CHECK(res->status == DB_RES_COMMAND_OK, "DB_exec failed");

	// This queue is because we can only run one query at a time,
	//   so we need to do the registration steps one at a time
	que_registration = Queue_create();

	SERROR_SALLOC(global_registration_watcher, sizeof(ev_check));
	ev_check_init(global_registration_watcher, registration_cb);
	ev_check_start(EV_A, global_registration_watcher);

	SERROR_SALLOC(global_notify_watcher, sizeof(ev_io));
	ev_io_init(global_notify_watcher, global_notify_cb, global_conn->int_sock, EV_READ);
	ev_io_start(EV_A, global_notify_watcher);

error:
	bol_query_in_process = false;
	DB_free_result(res);
	
	if (bol_error_state) {
		_str_response = DB_get_diagnostic(global_conn, res);
		SERROR_NORESPONSE("%s", _str_response);
	}
	SFREE(_str_response);
	bol_error_state = false;
	return true;
}

void global_notify_cb(EV_P, ev_io *w, int revents) {
	(void)w;
	(void)revents;

	int int_status = 1;

	if (socket_is_open(global_conn->int_sock) == false) {
		int_status = -1;
	} else if (!bol_query_in_process) {
		int_status = PQconsumeInput(global_conn->conn);
		if (int_status == 1) {
			send_notices(EV_A);

			PQisBusy(global_conn->conn);
			send_notices(EV_A);
		}
	}
	if (int_status != 1) {
		SERROR_NORESPONSE("Lost postgresql connection:\012%s", PQerrorMessage(global_conn->conn));
		SFREE(str_global_error);

		SERROR_CHECK(PQresetStart(global_conn->conn) == 1, "PQresetStart failed");

		ev_check_stop(EV_A, global_registration_watcher);
		ev_io_stop(EV_A, global_notify_watcher);

		SERROR_SALLOC(global_reconnect_timer, sizeof(ev_prepare));
		ev_prepare_init(global_reconnect_timer, client_reconnect_timer_cb);
		ev_prepare_start(EV_A, global_reconnect_timer);
		global_close_time = ev_now(EV_A);
		increment_idle(EV_A);

		// set up cnxn socket
		SERROR_SET_CONN_PQ_SOCKET(global_conn);
		ev_io_init(global_notify_watcher, global_conn_reset_cb, global_conn->int_sock, EV_WRITE);
		ev_io_start(EV_A, global_notify_watcher);
	}
error:
	bol_error_state = false;
}

void send_notices(EV_P) {
	PGnotify *pg_notify_current = NULL;
	pg_notify_current = PQnotifies(global_conn->conn);
	char *str_temp = NULL;
	char *str_id_literal = NULL;
	char *str_id = NULL;
	size_t int_temp_len;
	size_t int_query_len;
	char *str_query = NULL;

	while (pg_notify_current != NULL) {
		int_temp_len = pg_notify_current->extra != NULL ? strlen(pg_notify_current->extra) : 0;
		str_temp = bescape_value(pg_notify_current->extra != NULL ? pg_notify_current->extra : "", &int_temp_len);
		SERROR_CHECK(str_temp != NULL, "bescape_value failed");
		if (strncmp(str_temp, "NEW ", 4) == 0) {
			str_id_literal = DB_escape_literal(global_conn, str_temp + 4, int_temp_len - 4);
			SERROR_CHECK(str_id_literal != NULL, "DB_escape_literal failed");
			
			SERROR_SNCAT(str_id, &int_temp_len, str_temp + 4, int_temp_len - 4);

			SERROR_SNCAT(str_query, &int_query_len, str_global_query_get_notification, strlen(str_global_query_get_notification));
			SERROR_BREPLACE(str_query, &int_query_len, "{{NOTIFYID}}", str_id_literal, "g");

			bol_query_in_process = true;

			SERROR_CHECK(query_is_safe(str_query), "SQL Injection detected");
			SERROR_CHECK(DB_exec(EV_A, global_conn, str_id, str_query, notification_query_cb), "DB_exec failed");	

		} else if (strncmp(str_temp, "READ ", 5) == 0) {
			str_id_literal = DB_escape_literal(global_conn, str_temp + 5, int_temp_len - 5);
			SERROR_CHECK(str_id_literal != NULL, "DB_escape_literal failed");
			
			SERROR_SNCAT(str_id, &int_temp_len, str_temp + 5, int_temp_len - 5);

			SERROR_SNCAT(str_query, &int_query_len, str_global_query_get_profile_id_from_notification, strlen(str_global_query_get_profile_id_from_notification));
			SERROR_BREPLACE(str_query, &int_query_len, "{{NOTIFYID}}", str_id_literal, "g");

			bol_query_in_process = true;

			SERROR_CHECK(query_is_safe(str_query), "SQL Injection detected");
			SERROR_CHECK(DB_exec(EV_A, global_conn, str_id, str_query, notification_read_query_cb), "DB_exec failed");	

		} else {
			SERROR_NORESPONSE("BAD NOTIFY");
		}
		PQfreemem(pg_notify_current);

		pg_notify_current = PQnotifies(global_conn->conn);
	}
error:
	errno = 0;
	bol_error_state = false;
	if (pg_notify_current != NULL) {
		PQfreemem(pg_notify_current);
		pg_notify_current = NULL;
	}
	SFREE(str_id_literal);
	SFREE(str_temp);
	SFREE(str_query);
}

bool notification_query_cb(EV_P, void *cb_data, DB_result *res) {
	bool bol_ret = true;
	DArray *arr_values = NULL;
	char *_str_response = NULL;
	char *str_payload = NULL;
	char *str_title = NULL;
	char *str_body = NULL;
	char *str_apns_copy = NULL;
	char *str_profile_id = NULL;
	char *str_id = cb_data;
	char *str_timestamp = NULL;
	char *str_extra = NULL;
	size_t int_payload_len = 0;
	size_t int_title_len = 0;
	size_t int_body_len = 0;
	size_t int_apns_copy_len = 0;
	size_t int_profile_id_len = 0;
	size_t int_timestamp_len = 0;
	size_t int_extra_len = 0;

	DB_fetch_status status = DB_FETCH_OK;

	SERROR_CHECK(res != NULL, "DB_exec failed");
	SERROR_CHECK(res->status == DB_RES_TUPLES_OK, "DB_exec failed");

	char *str_payload1 = "{\"aps\":{\"alert\":{\"title\":{{TITLE}},\"body\":{{BODY}}},\"badge\":0,\"sound\":\"default\"},\"extra\":{{EXTRA}}}";

	while ((status = DB_fetch_row(res)) != DB_FETCH_END) {
		if (arr_values != NULL) {
			DArray_clear_destroy(arr_values);
		}
		arr_values = NULL;
		
		SERROR_CHECK(status != DB_FETCH_ERROR, "DB_fetch_row failed");

		arr_values = DB_get_row_values(res);
		SERROR_CHECK(arr_values != NULL, "DB_get_row_values failed");
		
		str_title = DArray_get(arr_values, 0);
		if (str_title != NULL) {
			int_title_len = strlen(str_title);
		} else {
			int_title_len = 0;
		}
		SERROR_JSONIFY(str_title, &int_title_len);
		
		str_body = DArray_get(arr_values, 1);
		if (str_body != NULL) {
			int_body_len = strlen(str_body);
		} else {
			int_body_len = 0;
		}
		SERROR_JSONIFY(str_body, &int_body_len);
		
		str_timestamp = DArray_get(arr_values, 4);
		if (str_timestamp != NULL) {
			int_timestamp_len = strlen(str_timestamp);
		} else {
			int_timestamp_len = 0;
		}
		SERROR_JSONIFY(str_timestamp, &int_timestamp_len);

		str_extra = DArray_get(arr_values, 5);
		if (str_extra != NULL) {
			int_extra_len = strlen(str_extra);
		} else {
			int_extra_len = 0;
		}
		SERROR_JSONIFY(str_extra, &int_extra_len);

		if (str_profile_id == NULL) {
			SERROR_SNCAT(str_profile_id, &int_profile_id_len, DArray_get(arr_values, 2), strlen(DArray_get(arr_values, 2)));
		}
		
		SERROR_SNCAT(str_payload, &int_payload_len, str_payload1, strlen(str_payload1));
		SERROR_BREPLACE(str_payload, &int_payload_len, "{{TITLE}}", str_title, "g");
		SERROR_BREPLACE(str_payload, &int_payload_len, "{{BODY}}", str_body, "g");
		SERROR_BREPLACE(str_payload, &int_payload_len, "{{EXTRA}}", str_extra, "g");
		
		if (DArray_get(arr_values, 3) != NULL) {
			SERROR_SNCAT(str_apns_copy, &int_apns_copy_len, DArray_get(arr_values, 3), strlen(DArray_get(arr_values, 3)));

			SERROR_CHECK(APNS_send(EV_A, notification_cb, str_apns_copy, DArray_get(arr_values, 3), strlen(DArray_get(arr_values, 3)), str_payload, int_payload_len), "Could not send notification");
			str_apns_copy = NULL;
		}
		
		SFREE(str_payload);
	}

	str_payload1 = "{\"title\":{{TITLE}},\"body\":{{BODY}},\"id\":{{ID}},\"timestamp\":{{TIMESTAMP}},\"extra\":{{EXTRA}}}";
	SERROR_SNCAT(str_payload, &int_payload_len, str_payload1, strlen(str_payload1));
	SERROR_BREPLACE(str_payload, &int_payload_len, "{{TITLE}}", str_title, "g");
	SERROR_BREPLACE(str_payload, &int_payload_len, "{{BODY}}", str_body, "g");
	SERROR_BREPLACE(str_payload, &int_payload_len, "{{ID}}", str_id, "g");
	SERROR_BREPLACE(str_payload, &int_payload_len, "{{TIMESTAMP}}", str_timestamp, "g");
	SERROR_BREPLACE(str_payload, &int_payload_len, "{{EXTRA}}", str_extra, "g");

	LIST_FOREACH(_server.list_client, first, next, client_node) {
		struct sock_ev_client *client = client_node->value;
		if (client->str_profile_id != NULL && strncmp(client->str_profile_id, str_profile_id, int_profile_id_len) == 0) {
			SERROR_CHECK(WS_sendFrame(EV_A, client, true, 0x01, str_payload, int_payload_len), "Failed to send message");
		}
	}


error:
	SFREE(str_apns_copy);

	bol_query_in_process = false;
	if (bol_error_state) {
		_str_response = DB_get_diagnostic(global_conn, res);
		SERROR_NORESPONSE("%s", _str_response);
	}
	DB_free_result(res);
	SFREE(_str_response);
	SFREE(str_payload);
	if (arr_values != NULL) {
		DArray_clear_destroy(arr_values);
		arr_values = NULL;
	}
	bol_error_state = false;
	return bol_ret;
}

void notification_cb(EV_P, APNS_notification_response *response, void *cb_data) {
	(void)EV_A;
	char *str_apns_id = cb_data;
	char *str_id_literal = NULL;
	char *str_query = NULL;
	size_t int_query_len;
	#ifdef UTIL_DEBUG
	size_t int_i = 0;
	while (int_i < DArray_count(response->arr_header)) {
		http2_header *header = DArray_get(response->arr_header, int_i);
		SDEBUG("%s: %s", header->str_name, header->str_value);

		int_i += 1;
	}
	SDEBUG("%s", response->str_payload);
	#endif
	if (strncmp(response->str_payload, "{\"reason\":\"BadDeviceToken\"}", strlen("{\"reason\":\"BadDeviceToken\"}")) == 0) {
		str_id_literal = DB_escape_literal(global_conn, str_apns_id, strlen(str_apns_id));
		SERROR_CHECK(str_id_literal != NULL, "DB_escape_literal failed");

		SERROR_SNCAT(str_query, &int_query_len, str_global_query_delete_device, strlen(str_global_query_delete_device));
		SERROR_BREPLACE(str_query, &int_query_len, "{{APNS}}", str_id_literal, "g");

		bol_query_in_process = true;
		SERROR_CHECK(query_is_safe(str_query), "SQL Injection detected");
		SERROR_CHECK(DB_exec(EV_A, global_conn, NULL, str_query, notification_apns_delete_query_cb), "DB_exec failed");
	}

error:
	SFREE(str_apns_id);
	SFREE(str_query);
	SFREE(str_id_literal);
}

bool notification_apns_delete_query_cb(EV_P, void *cb_data, DB_result *res) {
	(void)EV_A;
	(void)cb_data;
	bool bol_ret = true;
	char *_str_response = NULL;

	SERROR_CHECK(res != NULL, "DB_exec failed");
	SERROR_CHECK(res->status == DB_RES_COMMAND_OK, "DB_exec failed");

error:
	bol_query_in_process = false;
	if (bol_error_state) {
		_str_response = DB_get_diagnostic(global_conn, res);
		SERROR_NORESPONSE("%s", _str_response);
		bol_query_in_process = false;
	}
	bol_error_state = false;

	DB_free_result(res);
	SFREE(_str_response);
	bol_error_state = false;
	return bol_ret;
}

bool notification_unsent_query_cb(EV_P, void *cb_data, DB_result *res) {
	bool bol_ret = true;
	DArray *arr_values = NULL;
	char *_str_response = NULL;
	char *str_payload = NULL;
	char *str_title = NULL;
	char *str_body = NULL;
	char *str_timestamp = NULL;
	char *str_extra = NULL;
	Registration_action *action = cb_data;
	struct sock_ev_client *client = action->client;	
	size_t int_payload_len = 0;
	size_t int_title_len = 0;
	size_t int_body_len = 0;
	size_t int_timestamp_len = 0;
	size_t int_extra_len = 0;

	DB_fetch_status status = DB_FETCH_OK;

	SERROR_CHECK(res != NULL, "DB_exec failed");
	SERROR_CHECK(res->status == DB_RES_TUPLES_OK, "DB_exec failed");

	char *str_payload1 = "OLD {\"title\":{{TITLE}},\"body\":{{BODY}},\"id\":{{ID}},\"timestamp\":{{TIMESTAMP}},\"extra\":{{EXTRA}}}";

	while ((status = DB_fetch_row(res)) != DB_FETCH_END) {
		if (arr_values != NULL) {
			DArray_clear_destroy(arr_values);
		}
		arr_values = NULL;
		
		SERROR_CHECK(status != DB_FETCH_ERROR, "DB_fetch_row failed");

		arr_values = DB_get_row_values(res);
		SERROR_CHECK(arr_values != NULL, "DB_get_row_values failed");
		
		str_title = DArray_get(arr_values, 0);
		if (str_title != NULL) {
			int_title_len = strlen(str_title);
		} else {
			int_title_len = 0;
		}
		SERROR_JSONIFY(str_title, &int_title_len);

		str_body = DArray_get(arr_values, 1);
		int_body_len = strlen(DArray_get(arr_values, 1));
		if (str_body != NULL) {
			int_body_len = strlen(str_body);
		} else {
			int_body_len = 0;
		}
		SERROR_JSONIFY(str_body, &int_body_len);

		str_timestamp = DArray_get(arr_values, 3);
		if (str_timestamp != NULL) {
			int_timestamp_len = strlen(str_timestamp);
		} else {
			int_timestamp_len = 0;
		}
		SERROR_JSONIFY(str_timestamp, &int_timestamp_len);

		str_extra = DArray_get(arr_values, 4);
		if (str_extra != NULL) {
			int_extra_len = strlen(str_extra);
		} else {
			int_extra_len = 0;
		}
		SERROR_JSONIFY(str_extra, &int_extra_len);
		
		SERROR_SNCAT(str_payload, &int_payload_len, str_payload1, strlen(str_payload1));
		SERROR_BREPLACE(str_payload, &int_payload_len, "{{TITLE}}", str_title, "g");
		SERROR_BREPLACE(str_payload, &int_payload_len, "{{BODY}}", str_body, "g");
		SERROR_BREPLACE(str_payload, &int_payload_len, "{{ID}}", DArray_get(arr_values, 2), "g");
		SERROR_BREPLACE(str_payload, &int_payload_len, "{{TIMESTAMP}}", str_timestamp, "g");
		SERROR_BREPLACE(str_payload, &int_payload_len, "{{EXTRA}}", str_extra, "g");
		
		SERROR_CHECK(WS_sendFrame(EV_A, client, true, 0x01, str_payload, int_payload_len), "Failed to send message");

		SFREE(str_payload);
	}


error:
	bol_query_in_process = false;
	if (bol_error_state) {
		_str_response = DB_get_diagnostic(global_conn, res);
		SERROR_NORESPONSE("%s", _str_response);
	}
	DB_free_result(res);
	SFREE(_str_response);
	SFREE(str_payload);
	if (arr_values != NULL) {
		DArray_clear_destroy(arr_values);
		arr_values = NULL;
	}
	bol_error_state = false;
	return bol_ret;
}

bool notification_read_query_cb(EV_P, void *cb_data, DB_result *res) {
	bool bol_ret = true;
	DArray *arr_values = NULL;
	char *_str_response = NULL;
	char *str_payload = NULL;
	size_t int_payload_len = 0;
	char *str_id = cb_data;

	DB_fetch_status status = DB_FETCH_OK;

	SERROR_CHECK(res != NULL, "DB_exec failed");
	SERROR_CHECK(res->status == DB_RES_TUPLES_OK, "DB_exec failed");

	SERROR_SNCAT(str_payload, &int_payload_len, "READ ", (size_t)5, str_id, strlen(str_id));

	while ((status = DB_fetch_row(res)) != DB_FETCH_END) {
		if (arr_values != NULL) {
			DArray_clear_destroy(arr_values);
		}
		arr_values = NULL;
		
		SERROR_CHECK(status != DB_FETCH_ERROR, "DB_fetch_row failed");

		arr_values = DB_get_row_values(res);
		SERROR_CHECK(arr_values != NULL, "DB_get_row_values failed");
		
		LIST_FOREACH(_server.list_client, first, next, client_node) {
			struct sock_ev_client *client = client_node->value;
			if (client->str_profile_id != NULL && strncmp(client->str_profile_id, DArray_get(arr_values, 0), client->int_profile_id_len) == 0) {
				SERROR_CHECK(WS_sendFrame(EV_A, client, true, 0x01, str_payload, int_payload_len), "Failed to send message");
			}
		}
		SFREE(str_payload);
	}

error:
	SFREE(str_id);
	bol_query_in_process = false;
	if (bol_error_state) {
		_str_response = DB_get_diagnostic(global_conn, res);
		SERROR_NORESPONSE("%s", _str_response);
	}
	DB_free_result(res);
	SFREE(_str_response);
	SFREE(str_payload);
	if (arr_values != NULL) {
		DArray_clear_destroy(arr_values);
		arr_values = NULL;
	}
	bol_error_state = false;
	return bol_ret;
}

void client_reconnect_timer_cb(EV_P, ev_prepare *w, int revents) {
	(void)revents;

	if ((global_close_time + 30) < ev_now(EV_A)) {
		ev_io_stop(EV_A, global_notify_watcher);
		DB_finish(global_conn);

		ev_prepare_stop(EV_A, w);
		SFREE(global_reconnect_timer);

		decrement_idle(EV_A);

		SERROR_NORESPONSE("FATAL ERROR: could not re-connect to postgres in 30 seconds!");
		program_exit();
	}
}

void global_conn_reset_cb(EV_P, ev_io *w, int revents) {
	(void)revents;
	char *str_response = NULL;

	PostgresPollingStatusType status = PQresetPoll(global_conn->conn);

	// Some debug info
	switch (PQstatus(global_conn->conn)) {
		case CONNECTION_STARTED:
			SDEBUG("waiting for connection to be made.");
			break;
		case CONNECTION_MADE:
			SDEBUG("connection OK; waiting to send.");
			break;
		case CONNECTION_AWAITING_RESPONSE:
			SDEBUG("waiting for a response from the _server.");
			break;
		case CONNECTION_AUTH_OK:
			SDEBUG("received authentication; waiting for backend start-up to finish.");
			break;
		case CONNECTION_SSL_STARTUP:
			SDEBUG("negotiating SSL encryption.");
			break;
		case CONNECTION_SETENV:
			SDEBUG("negotiating environment-driven parameter settings.");
			break;
		case CONNECTION_OK:
			SDEBUG("CONNECTION_OK");
			break;
		case CONNECTION_BAD:
			SDEBUG("CONNECTION_BAD");
			break;
		case CONNECTION_NEEDED:
			SDEBUG("CONNECTION_NEEDED");
			break;
	}

	if (status == PGRES_POLLING_OK) {
		// Connection made
		SDEBUG("PGRES_POLLING_OK");
		ev_io_stop(EV_A, w);
		global_conn->int_status = 1;
		global_connect_cb(EV_A, NULL, global_conn);
		if (global_reconnect_timer != NULL) {
			ev_prepare_stop(EV_A, global_reconnect_timer);
			SFREE(global_reconnect_timer);

			decrement_idle(EV_A);
		}

	} else if (status == PGRES_POLLING_FAILED) {
		// Connection failed
		SDEBUG("PGRES_POLLING_FAILED");
		SFINISH_ERROR("Connect failed: %s", PQerrorMessage(global_conn->conn));

	} else if (status == PGRES_POLLING_READING) {
		// We want to read
		SDEBUG("PGRES_POLLING_READING");

		ev_io_stop(EV_A, w);
		ev_io_set(w, global_conn->int_sock, EV_READ);
		ev_io_start(EV_A, w);

	} else if (status == PGRES_POLLING_WRITING) {
		// We want to write
		SDEBUG("PGRES_POLLING_WRITING");

		ev_io_stop(EV_A, w);
		ev_io_set(w, global_conn->int_sock, EV_WRITE);
		ev_io_start(EV_A, w);
	}
	bol_error_state = false;
finish:
	if (str_response != NULL) {
		ev_io_stop(EV_A, w);
		global_conn->int_status = -1;
		SFREE(global_conn->str_response);
		global_conn->str_response = strdup(str_response + 6);
		SFREE(str_response);

		ev_prepare_stop(EV_A, global_reconnect_timer);
		SFREE(global_reconnect_timer);
		decrement_idle(EV_A);

		global_connect_cb(EV_A, NULL, global_conn);
	}
}
